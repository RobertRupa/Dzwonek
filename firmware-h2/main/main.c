#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_eventfd.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "openthread/dataset.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "openthread/udp.h"
#include "openthread/message.h"
#include "openthread/instance.h"

#define UPORT UART_NUM_1
#define BUFSIZE 640
static const char *TAG="doorbell_h2";
static bool ot_ready=false;
static otIp6Address dst;
static uint16_t dst_port=61631;
static bool dst_valid=false;
static otUdpSocket sock;
static bool sock_open=false;

static void uart_reply(const char *text) {uart_write_bytes(UPORT,text,strlen(text));uart_write_bytes(UPORT,"\n",1);}
static int nibble(char ch){if(ch>='0'&&ch<='9')return ch-'0';ch=tolower((unsigned char)ch);if(ch>='a'&&ch<='f')return ch-'a'+10;return -1;}
static const char *state_text(otDeviceRole role){
 switch(role){case OT_DEVICE_ROLE_DISABLED:return "disabled";case OT_DEVICE_ROLE_DETACHED:return "detached";case OT_DEVICE_ROLE_CHILD:return "child";case OT_DEVICE_ROLE_ROUTER:return "router";case OT_DEVICE_ROLE_LEADER:return "leader";default:return "unknown";}
}
static void handle(const char *cmd){
  if(!strncmp(cmd,"HELLO",5)){uart_reply("HELLO H2-Thread-Doorbell v1");return;}
  if(!ot_ready){uart_reply("ERR Thread initializing");return;}
  if(!esp_openthread_lock_acquire(pdMS_TO_TICKS(2000))){uart_reply("ERR lock timeout");return;}
  otInstance *ot=esp_openthread_get_instance();
  if(!strcmp(cmd,"STATUS")){
    char line[64];snprintf(line,sizeof(line),"STATE %s",state_text(otThreadGetDeviceRole(ot)));uart_reply(line);
  }else if(!strncmp(cmd,"TARGET ",7)){
    char ip[55];unsigned port=0;
    if(sscanf(cmd+7,"%54s %u",ip,&port)==2&&port>0&&port<=65535&&otIp6AddressFromString(ip,&dst)==OT_ERROR_NONE){dst_port=(uint16_t)port;dst_valid=true;uart_reply("OK TARGET");}
    else{dst_valid=false;uart_reply("ERR TARGET IPv6/port");}
  }else if(!strncmp(cmd,"DATASET ",8)){
    const char *hex=cmd+8;size_t n=strlen(hex);otOperationalDatasetTlvs d={0};
    if(n<2||n%2||n>sizeof(d.mTlvs)*2){uart_reply("ERR DATASET length");}
    else{
      bool ok=true;for(size_t i=0;i<n/2;i++){int a=nibble(hex[i*2]),b=nibble(hex[i*2+1]);if(a<0||b<0){ok=false;break;}d.mTlvs[i]=(uint8_t)((a<<4)|b);}
      if(ok){d.mLength=n/2;otError e=otDatasetSetActiveTlvs(ot,&d);if(e==OT_ERROR_NONE){otIp6SetEnabled(ot,true);otThreadSetEnabled(ot,true);uart_reply("OK DATASET");}else uart_reply("ERR DATASET rejected");}
      else uart_reply("ERR DATASET hex");
    }
  }else if(!strncmp(cmd,"RING ",5)){
    if(!dst_valid){uart_reply("ERR TARGET not set");}
    else if(otThreadGetDeviceRole(ot)==OT_DEVICE_ROLE_DISABLED||otThreadGetDeviceRole(ot)==OT_DEVICE_ROLE_DETACHED){uart_reply("ERR Thread not attached");}
    else{
      const char *payload=cmd+5;otMessage *msg=otUdpNewMessage(ot,NULL);
      if(!msg){uart_reply("ERR no memory");}
      else{
        otMessageInfo info={0};info.mPeerAddr=dst;info.mPeerPort=dst_port;
        otError e=otMessageAppend(msg,payload,strlen(payload));
        if(e==OT_ERROR_NONE)e=otUdpSend(ot,&sock,msg,&info);
        if(e!=OT_ERROR_NONE){otMessageFree(msg);uart_reply("ERR UDP send");}
        else uart_reply("OK RING");
      }
    }
  }else{uart_reply("ERR unknown command");}
  esp_openthread_lock_release();
}
static void serial_task(void *arg){
  char line[BUFSIZE];size_t n=0;uint8_t byte;
  while(1){int got=uart_read_bytes(UPORT,&byte,1,pdMS_TO_TICKS(100));if(got<=0)continue;
    if(byte=='\n') {line[n]=0;while(n&&line[n-1]=='\r')line[--n]=0;if(n)handle(line);n=0;}
    else if(n<sizeof(line)-1&&byte>=32&&byte<127){line[n++]=(char)byte;}
    else if(n>=sizeof(line)-1)n=0;
  }
}
static void openthread_task(void *arg){
  esp_openthread_platform_config_t cfg={
    .radio_config=ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
    .host_config=ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
    .port_config=ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
  };
  ESP_ERROR_CHECK(esp_openthread_init(&cfg));
  esp_netif_config_t ifcfg=ESP_NETIF_DEFAULT_OPENTHREAD();
  esp_netif_t *netif=esp_netif_new(&ifcfg);
  ESP_ERROR_CHECK(esp_netif_attach(netif,esp_openthread_netif_glue_init(&cfg)));
  ESP_ERROR_CHECK(esp_netif_set_default_netif(netif));
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance *instance=esp_openthread_get_instance();
  if(otUdpOpen(instance,&sock,NULL,NULL)==OT_ERROR_NONE)sock_open=true;
  otOperationalDatasetTlvs saved={0};
  if(otDatasetGetActiveTlvs(instance,&saved)==OT_ERROR_NONE&&saved.mLength){
    otIp6SetEnabled(instance,true);otThreadSetEnabled(instance,true);
    ESP_LOGI(TAG,"Loaded saved Thread dataset");
  }else ESP_LOGW(TAG,"Thread dataset absent; configure from web GUI on M5Stick");
  ot_ready=true;
  esp_openthread_lock_release();
  ESP_ERROR_CHECK(esp_openthread_launch_mainloop());
  ot_ready=false;
  esp_openthread_netif_glue_deinit();esp_netif_destroy(netif);
  vTaskDelete(NULL);
}
void app_main(void){
  esp_err_t e=nvs_flash_init();if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();}ESP_ERROR_CHECK(e);
  ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_vfs_eventfd_config_t eventcfg={.max_fds=4};ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventcfg));
  uart_config_t uc={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
  ESP_ERROR_CHECK(uart_param_config(UPORT,&uc));
  ESP_ERROR_CHECK(uart_set_pin(UPORT,CONFIG_DB_UART_TX,CONFIG_DB_UART_RX,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UPORT,1024,1024,0,NULL,0));
  ESP_LOGI(TAG,"UART RX=%d TX=%d",CONFIG_DB_UART_RX,CONFIG_DB_UART_TX);
  xTaskCreate(openthread_task,"ot_main",8192,NULL,5,NULL);
  xTaskCreate(serial_task,"stick_uart",4096,NULL,4,NULL);
}
