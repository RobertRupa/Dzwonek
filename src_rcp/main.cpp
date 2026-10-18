#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

// RCP diagnostic target. This is NOT an OpenThread host or Matter endpoint.
static constexpr int RCP_RX = 33, RCP_TX = 32;
static constexpr uint32_t RCP_BAUD = 460800;
HardwareSerial rcp(2);
WebServer web(80);
Preferences prefs;
WiFiUDP udp;
String ssid, password, apName, remoteHost;
uint16_t remotePort=61631;
uint32_t ringCount=0, lastButton=0, lastProbe=0, lastDraw=0;
bool apActive=false, mdnsActive=false, rcpResponded=false;
String rcpStatus="waiting", lastEvent="none";

// Spinel framing over HDLC-lite, CRC-16/CCITT (reflected 0x8408), initial FFFF.
static uint16_t hdlcFcs(const uint8_t* p,size_t len) {
  uint16_t fcs=0xffff;
  for(size_t i=0;i<len;i++) {
    fcs^=p[i];
    for(int b=0;b<8;b++) fcs=(fcs&1)?(fcs>>1)^0x8408:(fcs>>1);
  }
  return fcs;
}
static void hdlcSend(const uint8_t* p,size_t n) {
  auto byteOut=[](uint8_t c){
    if(c==0x7e||c==0x7d){rcp.write((uint8_t)0x7d);rcp.write((uint8_t)(c^0x20));}
    else rcp.write(c);
  };
  rcp.write((uint8_t)0x7e);
  for(size_t i=0;i<n;i++)byteOut(p[i]);
  uint16_t fcs=hdlcFcs(p,n)^0xffff;
  byteOut(fcs&0xff);byteOut(fcs>>8);
  rcp.write((uint8_t)0x7e);
}
static void probeRcp() {
  // Spinel header 0x81 (IID=0, TID=1), CMD_RESET = 1.
  const uint8_t reset[]={0x81,0x01};
  hdlcSend(reset,sizeof(reset));
  lastProbe=millis();
  rcpStatus="probing";
}
static void readRcp() {
  static uint8_t data[512];
  static size_t n=0;
  static bool escaped=false, overflowing=false;
  while(rcp.available()) {
    uint8_t c=rcp.read();
    if(c==0x7e) {
      if(n>=4 && !overflowing && hdlcFcs(data,n)==0xf0b8) {
        rcpResponded=true;
        rcpStatus="HDLC frame OK";
        // Do not claim a working Thread link: a valid Spinel frame merely proves serial transport.
      }
      n=0;escaped=false;overflowing=false;
      continue;
    }
    if(c==0x7d&&!escaped){escaped=true;continue;}
    if(escaped){c^=0x20;escaped=false;}
    if(!overflowing) {if(n<sizeof(data))data[n++]=c;else overflowing=true;}
  }
}
static String escapeHtml(String s){
 s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");return s;
}
static String page(){
 String h="<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Doorbell RCP</title></head><body><h2>M5StickC Plus2 / H2 RCP</h2>";
 h+="<p>WiFi: "+escapeHtml(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"offline")+"</p>";
 h+="<p>RCP: "+escapeHtml(rcpStatus)+" (serial diagnostic only)</p>";
 h+="<p>Rings: "+String(ringCount)+"; last: "+escapeHtml(lastEvent)+"</p>";
 h+="<form method='post' action='/ring'><button>Ring (WiFi UDP)</button></form>";
 h+="<form method='post' action='/probe'><button>Probe H2 RCP</button></form>";
 h+="<h3>Settings</h3><form method='post' action='/save'>";
 h+="Name/hostname is not used in event format.<p>WiFi SSID <input name='ssid' value='"+escapeHtml(ssid)+"'></p>";
 h+="<p>WiFi password <input type='password' name='pass' placeholder='unchanged if empty'></p>";
 h+="<p>UDP destination IP / hostname <input name='host' value='"+escapeHtml(remoteHost)+"'></p>";
 h+="<p>UDP port <input type='number' min='1' max='65535' name='port' value='"+String(remotePort)+"'></p>";
 h+="<button>Save and restart</button></form>";
 h+="<p>H2 uses Spinel/HDLC at 460800 baud. No Matter commissioning or Thread data plane on Stick in this target.</p>";
 h+="<p>Setup AP has no password. Do not expose on an untrusted network.</p></body></html>";
 return h;
}
static void ring(){
 ringCount++;M5.Speaker.tone(1800,130);
 if(WiFi.status()!=WL_CONNECTED||remoteHost.isEmpty()){lastEvent="not sent: no WiFi/destination";return;}
 String message="DOORBELL,StickPlus2,"+String(ringCount);
 if(udp.beginPacket(remoteHost.c_str(),remotePort)) {
   udp.print(message);
   lastEvent=udp.endPacket()?"UDP sent (unconfirmed)":"UDP failure";
 } else lastEvent="UDP destination resolution failed";
 Serial.println(lastEvent);
}
void setup(){
 Serial.begin(115200);
 auto cfg=M5.config();cfg.internal_spk=true;M5.begin(cfg);
 M5.Display.setRotation(0);
 prefs.begin("doorbell-rcp",false);
 ssid=prefs.getString("ssid","");
 password=prefs.getString("pass","");
 remoteHost=prefs.getString("host","");
 remotePort=prefs.getUShort("port",61631);
 uint64_t mac=ESP.getEfuseMac();char suffix[5];
 snprintf(suffix,sizeof(suffix),"%04X",(unsigned)(mac&0xffff));
 apName="Doorbell-RCP-"+String(suffix);
 WiFi.mode(WIFI_AP_STA);
 apActive=WiFi.softAP(apName.c_str());
 if(ssid.length())WiFi.begin(ssid.c_str(),password.c_str());
 rcp.begin(RCP_BAUD,SERIAL_8N1,RCP_RX,RCP_TX);
 probeRcp();
 web.on("/",HTTP_GET,[]{web.send(200,"text/html; charset=utf-8",page());});
 web.on("/ring",HTTP_POST,[]{ring();web.sendHeader("Location","/");web.send(303);});
 web.on("/probe",HTTP_POST,[]{probeRcp();web.sendHeader("Location","/");web.send(303);});
 web.on("/api/status",HTTP_GET,[]{web.send(200,"application/json",String("{\"rcp_frame_received\":")+(rcpResponded?"true":"false")+",\"rings\":"+String(ringCount)+"}");});
 web.on("/save",HTTP_POST,[]{
   String port=web.arg("port");long p=port.toInt();
   if(p<1||p>65535||web.arg("ssid").length()>32||web.arg("host").length()>253){web.send(400,"text/plain","Invalid settings");return;}
   prefs.putString("ssid",web.arg("ssid"));
   if(web.arg("pass").length())prefs.putString("pass",web.arg("pass"));
   prefs.putString("host",web.arg("host"));prefs.putUShort("port",(uint16_t)p);
   web.send(200,"text/plain","Saved. Rebooting");delay(300);ESP.restart();
 });
 web.begin();
 Serial.printf("RCP diagnostics AP: %s / http://192.168.4.1/\n",apName.c_str());
}
void loop(){
 M5.update();web.handleClient();readRcp();
 if(M5.BtnA.wasPressed()&&millis()-lastButton>300){lastButton=millis();ring();}
 if(M5.BtnB.wasPressed())probeRcp();
 if(millis()-lastProbe>12000&&!rcpResponded)probeRcp();
 if(WiFi.status()==WL_CONNECTED&&!mdnsActive)mdnsActive=MDNS.begin("doorbell-rcp");
 if(millis()-lastDraw>600){
  lastDraw=millis();M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE,TFT_BLACK);M5.Display.setTextSize(1);
  M5.Display.setCursor(5,10);M5.Display.println("DOORBELL / H2 RCP");
  M5.Display.setCursor(5,30);M5.Display.printf("WiFi: %s",WiFi.status()==WL_CONNECTED?"OK":"OFF");
  M5.Display.setCursor(5,48);M5.Display.printf("RCP: %s",rcpResponded?"HDLC OK":"no frame");
  M5.Display.setCursor(5,67);M5.Display.printf("Rings: %lu",(unsigned long)ringCount);
  M5.Display.setCursor(5,95);M5.Display.println("A: UDP bell");
  M5.Display.setCursor(5,110);M5.Display.println("B: Probe RCP");
  M5.Display.setCursor(5,145);M5.Display.println("AP: 192.168.4.1");
 }
 delay(5);
}
