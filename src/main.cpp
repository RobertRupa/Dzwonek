#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <qrcode.h>

// Grove port on M5StickC Plus2: MCU RX=33 (white / H2 TX), MCU TX=32 (yellow / H2 RX).
static constexpr int H2_RX=33, H2_TX=32;
static constexpr uint32_t H2_BAUD=115200;
HardwareSerial h2(2);
WebServer server(80);
Preferences prefs;
String ssid, password, targetIPv6, deviceName, h2Dataset;
uint16_t destPort=61631;
bool apActive=false, mdnsActive=false;
uint32_t presses=0, lastPress=0, lastH2Status=0, lastScreen=0, lastWifiCheck=0;
String h2Status="unknown", h2Last="none", apName;
String matterPin="", matterManual="", matterQr="", matterState="waiting for H2";
String htmlEscape(String s);
uint32_t lastMatterQuery=0;
uint8_t screenPage=0;
String matterHtml() {
  String x="<div class='card'><h2>Matter / Thread</h2>";
  x+="<p>H2: "+htmlEscape(matterState)+"</p>";
  if(matterQr.length()) {
    x+="<p><b>PIN:</b> "+htmlEscape(matterPin)+"<br><b>Kod ręczny:</b> "+htmlEscape(matterManual)+"</p>";
    x+="<img alt='Kod QR Matter' src='/matter.svg' width='235' height='235'>";
    x+="<p><small>Payload: "+htmlEscape(matterQr)+"</small></p>";

  } else x+="<p>Nie ma kodu parowania. Sprawdź firmware Matter H2 i UART.</p>";
  x+="<form method='POST' action='/matter/query'><button>Odczytaj kod z H2</button></form>";
  x+="<p><small>PIN/QR muszą pochodzić z firmware H2. Domyślny kod testowy ESP-Matter nie nadaje się do produktu końcowego.</small></p></div>";
  return x;
}

String htmlEscape(String s) {
  s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&#39;");return s;
}
String formPage(const String& notice="") {
  String x=R"HTML(<!doctype html><html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Thread Doorbell</title><style>body{font:16px system-ui;background:#101827;color:#e9f0fa;margin:0;padding:22px}main{max-width:600px;margin:auto}.card{background:#1f2c40;border-radius:15px;padding:22px;margin:16px 0}input{display:block;width:100%;box-sizing:border-box;margin:7px 0 15px;background:#101827;border:1px solid #607089;border-radius:8px;color:white;padding:12px}label{display:block}button{background:#34c3a4;border:0;padding:12px 18px;border-radius:8px;font-weight:bold;cursor:pointer}small{color:#bdcadd}a{color:#83e6d3}</style></head><body><main><h1>Thread Doorbell</h1>)HTML";
  if (notice.length()) x+="<p class='card'>"+htmlEscape(notice)+"</p>";
  x+="<div class='card'><b>Status</b><p>Wi-Fi: "+htmlEscape(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"offline")+"<br>AP: "+htmlEscape(apActive?apName:"wyłączony")+"<br>H2: "+htmlEscape(h2Status)+"<br>Naciśnięcia: "+String(presses)+"</p><form method='POST' action='/ring'><button>Test dzwonka</button></form></div>";
  x+=matterHtml();
  x+="<div class='card'><h2>Konfiguracja</h2><form method='POST' action='/save'>";
  x+="<label>Nazwa urządzenia<input name='name' maxlength='30' value='"+htmlEscape(deviceName)+"' required></label>";
  x+="<label>SSID Wi-Fi<input name='ssid' maxlength='32' value='"+htmlEscape(ssid)+"'></label>";
  x+="<label>Hasło Wi-Fi (puste = pozostaw poprzednie)<input type='password' name='password' maxlength='63' autocomplete='new-password'></label>";
  x+="<label>Docelowy adres IPv6 w sieci Thread<input name='target' maxlength='45' value='"+htmlEscape(targetIPv6)+"' placeholder='fd12:3456::1234'></label>";
  x+="<label>Port UDP<input type='number' min='1' max='65535' name='port' value='"+String(destPort)+"'></label>";
  x+="<label>Thread Active Dataset TLVs (hex)<input name='dataset' maxlength='510' value='"+htmlEscape(h2Dataset)+"' placeholder='Opcjonalnie: eksport z Border Router'></label>";
  x+="<button type='submit'>Zapisz w NVS i uruchom ponownie</button></form><p><small>AP jest otwarte i bez hasła. Nie używaj w niezaufanej lokalizacji. Hasło Wi-Fi zostanie zapisane w NVS (domyślnie bez szyfrowania pamięci flash).</small></p></div>";
  x+="<div class='card'><a href='/api/status'>Status JSON</a> · <a href='/'>Odśwież</a></div></main></body></html>";
  return x;
}
void sendH2(const String& command) { h2.print(command);h2.print('\n'); Serial.printf("H2 > %s\n",command.c_str()); }
void ring() {
  presses++;
  String payload="DOORBELL,"+deviceName+","+String(presses);
  sendH2("RING "+payload);
  M5.Speaker.tone(1800,130);
  Serial.printf("RING #%lu\n",static_cast<unsigned long>(presses));
}
void loadConfig(){
  prefs.begin("doorbell",false);
  ssid=prefs.getString("ssid","");password=prefs.getString("pass","");
  targetIPv6=prefs.getString("target","");destPort=prefs.getUShort("port",61631);
  deviceName=prefs.getString("name","Doorbell");h2Dataset=prefs.getString("dataset","");
}
void startAP(){
  uint64_t mac=ESP.getEfuseMac();char suffix[5];snprintf(suffix,sizeof(suffix),"%04X",(unsigned)(mac&0xffff));
  apName="Doorbell-Setup-"+String(suffix);
  WiFi.mode(WIFI_AP_STA);
  apActive=WiFi.softAP(apName.c_str());
  Serial.printf("AP: %s  IP: %s  [BRAK HASLA]\n",apName.c_str(),WiFi.softAPIP().toString().c_str());
}
void connectWifi(){
  if(ssid.isEmpty())return;
  WiFi.mode(apActive?WIFI_AP_STA:WIFI_STA);
  WiFi.begin(ssid.c_str(),password.c_str());
  Serial.printf("Wi-Fi connecting: %s\n",ssid.c_str());
}
void initHttp(){
  server.on("/",HTTP_GET,[]{server.send(200,"text/html; charset=utf-8",formPage());});
  server.on("/matter.svg",HTTP_GET,[]{
    if(matterQr.isEmpty()){server.send(404,"text/plain","No payload");return;}
    uint8_t buf[qrcode_getBufferSize(3)];QRCode q;
    if(qrcode_initText(&q,buf,3,ECC_LOW,matterQr.c_str())!=0){server.send(500,"text/plain","QR generation failed");return;}
    String svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='-4 -4 "+String(q.size+8)+" "+String(q.size+8)+"' shape-rendering='crispEdges'><rect x='-4' y='-4' width='"+String(q.size+8)+"' height='"+String(q.size+8)+"' fill='white'/>";
    for(int y=0;y<q.size;y++)for(int x=0;x<q.size;x++)if(qrcode_getModule(&q,x,y))svg+="<rect x='"+String(x)+"' y='"+String(y)+"' width='1' height='1' fill='black'/>";
    svg+="</svg>";server.send(200,"image/svg+xml",svg);
  });
  server.on("/matter/query",HTTP_POST,[]{sendH2("MATTER INFO");server.sendHeader("Location","/");server.send(303);});
  server.on("/ring",HTTP_POST,[]{ring();server.sendHeader("Location","/");server.send(303);});
  server.on("/api/status",HTTP_GET,[]{
    String json="{\"wifi\":"+String(WiFi.status()==WL_CONNECTED?"true":"false")+",\"ip\":\""+WiFi.localIP().toString()+"\",\"h2\":\""+h2Status+"\",\"presses\":"+String(presses)+"}";
    server.send(200,"application/json",json);
  });
  server.on("/save",HTTP_POST,[]{
    if(!server.hasArg("name")||!server.hasArg("port")){server.send(400,"text/plain","Invalid request");return;}
    String p=server.arg("port"), name=server.arg("name"), dataset=server.arg("dataset"), target=server.arg("target");
    int port=p.toInt();if(port<1||port>65535||name.isEmpty()||dataset.length()>510){server.send(400,"text/plain","Invalid configuration");return;}
    for(size_t i=0;i<dataset.length();i++)if(!isxdigit((unsigned char)dataset[i])){server.send(400,"text/plain","Invalid dataset hex");return;}
    if(dataset.length()%2){server.send(400,"text/plain","Dataset must contain complete bytes");return;}
    prefs.putString("name",name);prefs.putString("ssid",server.arg("ssid"));
    if(server.arg("password").length())prefs.putString("pass",server.arg("password"));
    prefs.putString("target",target);prefs.putUShort("port",port);prefs.putString("dataset",dataset);
    server.send(200,"text/html; charset=utf-8",formPage("Zapisano w NVS. Restart urządzenia za 2 sekundy."));
    Serial.println("Settings saved to NVS");delay(2000);ESP.restart();
  });
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
  server.begin();
}
void drawScreen(){
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE,TFT_BLACK);
  M5.Display.setTextSize(1);
  if(screenPage == 1) {
    M5.Display.setCursor(7,6);M5.Display.println("MATTER PAIRING");
    if(matterQr.length()) {
      M5.Display.setCursor(7,24);M5.Display.printf("PIN: %s",matterPin.c_str());
      // QRCode v3 fits payload of standard ESP-Matter setup text.
      uint8_t data[qrcode_getBufferSize(3)]; QRCode qr;
      int result=qrcode_initText(&qr,data,3,ECC_LOW,matterQr.c_str());
      if(result==0) {int module=3;int side=qr.size*module;int ox=(M5.Display.width()-side)/2;int oy=49;
        M5.Display.fillRect(ox-3,oy-3,side+6,side+6,TFT_WHITE);
        for(int y=0;y<qr.size;y++) for(int x=0;x<qr.size;x++) if(qrcode_getModule(&qr,x,y)) M5.Display.fillRect(ox+x*module,oy+y*module,module,module,TFT_BLACK);
      } else {M5.Display.setCursor(7,53);M5.Display.println("QR error");}
    } else {M5.Display.setCursor(7,34);M5.Display.println("H2: no Matter code");}
    M5.Display.setCursor(7,186);M5.Display.println("B = main screen");
    return;
  }
  M5.Display.setCursor(7,8);M5.Display.println("THREAD DOORBELL");
  M5.Display.setCursor(7,32);M5.Display.printf("WiFi: %s\n",WiFi.status()==WL_CONNECTED?"OK":"OFF");
  M5.Display.setCursor(7,48);M5.Display.printf("Thread: %s\n",h2Status.c_str());
  M5.Display.setCursor(7,64);M5.Display.printf("Rings: %lu\n",(unsigned long)presses);
  M5.Display.setCursor(7,90);M5.Display.println("A = RING");
  M5.Display.setCursor(7,106);M5.Display.println("B = INFO");
  M5.Display.setCursor(7,135);M5.Display.println(apActive?"AP:192.168.4.1":"AP disabled");
  if(WiFi.status()==WL_CONNECTED){M5.Display.setCursor(7,151);M5.Display.println(WiFi.localIP().toString());}
}
void pollH2(){
  static String line;
  while(h2.available()){
    char c=(char)h2.read();if(c=='\n'){
      line.trim();if(line.startsWith("STATE ")) h2Status=line.substring(6);
      if(line.startsWith("MATTER ")) {
        // Format: MATTER <PIN> <MANUAL_CODE> <QR_PAYLOAD> <STATE>
        int a=line.indexOf(' ',7),b=a<0?-1:line.indexOf(' ',a+1),c=b<0?-1:line.indexOf(' ',b+1);
        if(a>0&&b>a&&c>b){matterPin=line.substring(7,a);matterManual=line.substring(a+1,b);matterQr=line.substring(b+1,c);matterState=line.substring(c+1);}
      }
      if(line.startsWith("MATTER_STATE "))matterState=line.substring(13);
      h2Last=line;Serial.printf("H2 < %s\n",line.c_str());line="";
    }else if(c!='\r'&&line.length()<250)line+=c;
  }
}
void setup(){
  Serial.begin(115200);
  auto cfg=M5.config();cfg.internal_spk=true;M5.begin(cfg);
  M5.Display.setRotation(0);M5.Display.setTextSize(1);
  loadConfig();h2.begin(H2_BAUD,SERIAL_8N1,H2_RX,H2_TX);
  WiFi.persistent(false);startAP();connectWifi();initHttp();
  delay(350);sendH2("HELLO");sendH2("MATTER INFO");
  if(h2Dataset.length()) sendH2("DATASET "+h2Dataset);
  if(targetIPv6.length())sendH2("TARGET "+targetIPv6+" "+String(destPort));
  drawScreen();
}
void loop(){
  M5.update();server.handleClient();pollH2();
  if(M5.BtnA.wasPressed()&&millis()-lastPress>300){lastPress=millis();ring();}
  if(M5.BtnB.wasPressed()){screenPage^=1;sendH2("MATTER INFO");Serial.printf("STATUS wifi=%d h2=%s ip=%s\n",WiFi.status(),h2Status.c_str(),WiFi.localIP().toString().c_str());sendH2("STATUS");}
  if(millis()-lastMatterQuery>15000 && matterQr.isEmpty()){lastMatterQuery=millis();sendH2("MATTER INFO");}
  if(millis()-lastH2Status>10000){lastH2Status=millis();sendH2("STATUS");}
  if(millis()-lastWifiCheck>15000){lastWifiCheck=millis();if(WiFi.status()!=WL_CONNECTED&&ssid.length())connectWifi();if(WiFi.status()==WL_CONNECTED&&!mdnsActive){mdnsActive=MDNS.begin("thread-doorbell");}}
  if(millis()-lastScreen>1200){lastScreen=millis();drawScreen();}
  delay(5);
}
