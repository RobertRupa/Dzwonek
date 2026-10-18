// UART bridge for the official esp-matter light_switch reference firmware.
// WARNING: The codes below are the OFFICIAL DEFAULT TEST setup credentials.
// If factory commissioning data/passcode is changed, these values MUST be
// updated from that source. Never ship common development credentials.
#include <cstdio>
#include <cstring>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "doorbell_uart.h"

static constexpr uart_port_t kPort = UART_NUM_1;
static constexpr char kPin[]="20202021";
static constexpr char kManual[]="34970112332";
static constexpr char kQr[]="MT:Y.K9042C00KA0648G00";
static void reply(const char *s) {
    uart_write_bytes(kPort,s,strlen(s));uart_write_bytes(kPort,"\n",1);
}
static void task(void *) {
    char line[160]; size_t n=0; uint8_t ch;
    for(;;) {
        if(uart_read_bytes(kPort,&ch,1,pdMS_TO_TICKS(50))<=0)continue;
        if(ch=='\r')continue;
        if(ch=='\n') {
            line[n]=0;
            if(strcmp(line,"MATTER INFO")==0) {
                char result[160];
                snprintf(result,sizeof(result),"MATTER %s %s %s available",kPin,kManual,kQr);
                reply(result);
            } else if(strcmp(line,"HELLO")==0)reply("HELLO H2 ESP-MATTER");
            else if(strcmp(line,"STATUS")==0)reply("STATE matter");
            else if(strncmp(line,"RING ",5)==0) {
                doorbell_matter_press();
                reply("OK RING MATTER SWITCH");
            } else if(strncmp(line,"TARGET ",7)==0 || strncmp(line,"DATASET ",8)==0) {
                reply("ERR Legacy UDP commands unavailable in Matter mode");
            } else if(n)reply("ERR Unknown Matter UART command");
            n=0;
        } else if(ch>=32 && ch<=126 && n<sizeof(line)-1)line[n++]=(char)ch;
        else if(n>=sizeof(line)-1)n=0;
    }
}
extern "C" void doorbell_uart_init(void) {
    uart_config_t c={}; c.baud_rate=115200; c.data_bits=UART_DATA_8_BITS;
    c.parity=UART_PARITY_DISABLE;c.stop_bits=UART_STOP_BITS_1;
    c.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;c.source_clk=UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(kPort,&c));
    ESP_ERROR_CHECK(uart_set_pin(kPort,CONFIG_DB_H2_UART_TX,CONFIG_DB_H2_UART_RX,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(kPort,512,512,0,nullptr,0));
    xTaskCreate(task,"doorbell_uart",4096,nullptr,4,nullptr);
}
