#include <WiFi.h>
#include <ESPAsyncWebServer.h>
//#include <WiFiClient.h>
//#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <EEPROM.h>

#include "settings.h" //ESP32 Settings
#include "index.h"  //Index/intro page
#include "update.h" //Update page
#include "ws.h" // Websocket client page
#include "connect.h" // Wifi connect page
#include "evs.h" // Event Source page
#include "sc.h" // State Changer page
#include "hegvid.h" //HEGstudio clone (WIP)
#include "graphing.h" //WebGL graphing (WIP)

#include "HEG.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient * globalWSClient = NULL;
AsyncEventSource events("/events");

//Enter your WiFi SSID and PASSWORD
const char* host = "esp32";
const char* softAPName = "My_HEG";

char received;

size_t content_len;
unsigned long t_start,t_stop;

String setSSID;
String setPass;
String myLocalIP;
String staticIP;
String gateway;
String subnetM;

void saveWiFiLogin(bool ap_only, bool use_static, bool reset){
  //512 usable address bytes [default values: 0x00], each char uses a byte.
  EEPROM.begin(512);
  //Store SSID at address 1
  int address = 2;
  //Serial.print("Previous saved ssid: ");
  //Serial.println(EEPROM.readString(address));
  Serial.print("New saved SSID: ");
  Serial.println(setSSID);
  EEPROM.writeString(address, setSSID);
  //Store password at address 64
  address = 128;
  //Serial.print("Previous saved password: ");
  //Serial.println(EEPROM.readString(address));
  Serial.print("New saved password: ");
  Serial.println(setPass);
  EEPROM.writeString(address, setPass);
  if(use_static == true){
    EEPROM.write(255, 1);
    EEPROM.writeString(256, staticIP);
    EEPROM.writeString(320, gateway);
    EEPROM.writeString(384, subnetM);
  }
  else { EEPROM.write(255,0); EEPROM.writeString(256, ""); EEPROM.writeString(320, ""); EEPROM.writeString(384, "");}
  if(ap_only == true){ EEPROM.write(1, 0); } //Address of whether to attempt a connection by default.
  else { EEPROM.write(1,1); }
  Serial.println("Committing to flash...");
  EEPROM.commit();
  delay(100);
  EEPROM.end();
  
  if(reset == true){ Serial.println("Resetting ESP32..."); ESP.restart();} //Reset to trigger auto-connect
}

void connectAP(){
  //ESP32 As access point IP: 192.168.4.1
  Serial.println("Starting local access point, scan for StateChanger in available WiFi connections");
  Serial.println("Log in at 192.168.4.1 after connecting successfully");
  //WiFi.mode(WIFI_AP); //Access Point mode, creates a local access point
  WiFi.softAP(softAPName, "12345678");    //Password length minimum 8 char 
  myLocalIP = "192.168.4.1";
}

IPAddress parseIP(String ipString){ //Parse IP from string
  int i = ipString.indexOf('.');
  String temp1 = ipString.substring(0,i);
  int j = ipString.indexOf('.', i+1);
  String temp2 = ipString.substring(i+1,j);
  int k = ipString.indexOf('.', j+1);
  String temp3 = ipString.substring(j+1,k);
  String temp4 = ipString.substring(k+1);
  return IPAddress(temp1.toInt(),temp2.toInt(),temp3.toInt(),temp4.toInt());
}

void setupStation(bool use_static){
  Serial.println("Setting up WiFi Connection...");
  //Serial.println("Disconnecting from previous network...");
  //WiFi.softAPdisconnect();
  //WiFi.disconnect();
  //WiFi.mode(WIFI_OFF);
  //delay(1000);
  
  //WiFi.mode(WIFI_STA);
  if(use_static == true) {
    //str.split('.');
    if(staticIP.indexOf('.') != -1) {
      IPAddress staticIPAddress = parseIP(staticIP);
      IPAddress gateWay = parseIP(gateway);
      IPAddress subnet_Mask = parseIP(subnetM);
      Serial.print("Static IP: ");
      Serial.println(staticIPAddress);
      Serial.print("Gateway IP: ");
      Serial.println(gateWay);
      Serial.print("Subnet Mask: ");
      Serial.println(subnet_Mask);
      WiFi.config(staticIPAddress, gateWay, subnet_Mask);
    }  
    else { 
      Serial.println("No saved Static IP.");
    }
  }
  Serial.println("Connecting to SSID: ");
  Serial.print(setSSID);
  WiFi.begin(setSSID.c_str(),setPass.c_str());
  int wait = 0;
  while((WiFi.waitForConnectResult() != WL_CONNECTED)){
        if(wait >= 0){ break; }     
        Serial.print("...");
        wait++;
        delay(100);
      }
  if(WiFi.waitForConnectResult() == WL_CONNECTED){
    //If connection successful show IP address in serial monitor
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(setSSID);
    Serial.print("IP address: ");
    myLocalIP = WiFi.localIP().toString();
    Serial.println(myLocalIP);  //IP address assigned to your ESP
    Serial.println("Connect to host and access via the new local IP assigned to the ESP32");
  }
  else{
    WiFi.disconnect();
    Serial.println("");
    Serial.println("Connection Failed.");
    Serial.print("Attempted at SSID: ");
    Serial.println(setSSID);
    delay(100);
    connectAP();
  }
}

//ESP32 COMMAND MODULE

void commandESP32(char received)
{
  if (received == 't')
  { //Enable Sensor
    coreProgramEnabled = true;
    digitalWrite(LED, LOW);
  }
  if (received == 'f')
  { //Disable sensor, reset.
    coreProgramEnabled = false;
    digitalWrite(LED, HIGH);
    digitalWrite(RED, LOW);
    digitalWrite(IR, LOW);
    reset = true;
  }
  if (received == 'D'){ // for use with a Serial Monitor
    if(coreProgramEnabled = false){
      coreProgramEnabled = true;
      if(DEBUG_LEDS==false){
        DEBUG_LEDS = true;
        DEBUG_ADC = true;
      }
      else{
        DEBUG_LEDS = false;
        DEBUG_ADC = false;
      }
    }
    else{
      coreProgramEnabled = false;
      DEBUG_LEDS = false;
      DEBUG_ADC = false;
    }
    
    
  }
  if (received == 's')
  { //Reset baseline and readings
    reset = true;
  }
  if (received == 'R') {
    if (USE_USB == true) {
      Serial.println("Restarting ESP32...");
    }
    /*if (USE_BT == true) {
      if (SerialBT.hasClient() == true) {
        SerialBT.println("Restarting ESP32...");
      }
    }*/
    delay(300);
    ESP.restart();
  }
  if (received == 'p')
  { //pIR Toggle
    pIR_MODE = true;
    digitalWrite(RED, LOW);
    reset = true;
  }
  if (received == 'u')
  { //USB Toggle
    if (USE_USB == false)
    {
      USE_USB = true;
      Serial.begin(115200);
    }
    else
    {
      USE_USB = false; // Serial.end() or Serial.hasClient() type function to suppress serial output?
    }
  }
  if (received == 'b')
  { //Bluetooth Toggle
    EEPROM.begin(512);
    if (EEPROM.read(0) == 0)
    {
      EEPROM.write(0,1);
      EEPROM.commit();
      EEPROM.end();
      delay(100);
      ESP.restart();
    }
    else
    {
      EEPROM.write(0,0);
      EEPROM.commit();
      EEPROM.end();
      delay(100);
      ESP.restart();
    }
  }
  if (received == 'T') {
    sensorTest();
    LEDTest();
  }
  if ((received == '0') || (received == '1') || (received == '2') || (received == '3'))
  {
    adcChannel = received - '0';
    reset = true;
  }
  if (received == 'n') {
    if (NOISE_REDUCTION == false) {
      NOISE_REDUCTION = true;
    }
    if (NOISE_REDUCTION == true) {
      NOISE_REDUCTION = false;
    }
    reset = true;
  }
  if (received == 'r')
  { // Dual Sensor toggle, changes LED pinouts and adcChannel to left or right.
    digitalWrite(RED, LOW);
    digitalWrite(IR, LOW);
    IR = IR3;
    RED = RED3;
    IRn = IR2;
    REDn = RED2;
    adcChannel = 1;
    pinMode(IR, OUTPUT);
    pinMode(RED, OUTPUT);
    reset = true;
  }
  if (received == 'l') {
    digitalWrite(RED, LOW);
    digitalWrite(IR, LOW);
    IR = IR0;
    RED = RED0;
    IRn = IR1;
    REDn = RED1;
    adcChannel = 0;
    reset = true;
  }
  if (received == 'c') { // Toggle center pins
    digitalWrite(RED, LOW);
    digitalWrite(IR, LOW);
    RED = RED0;
    IR = IR0;
    REDn = RED2;
    IRn = RED2;
    adcChannel = 1;
    reset = true;
  }
  delay(500);
}

// HTTP server GET/POST handles.

//Return JSON string of WiFi scan results
String scanWiFi(){
  String json = "<div>[";
  int n = WiFi.scanComplete();
  if(n == -2){
    WiFi.scanNetworks(true); //WiFi.scanNetworks(true,true); // Exposes hidden networks
  } else if(n){
    for (int i = 0; i < n; ++i){
      if(i) json += ",";
      json += "{";
      json += "\"rssi\":"+String(WiFi.RSSI(i));
      json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
      json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
      json += ",\"channel\":"+String(WiFi.channel(i));
      json += ",\"secure\":"+String(WiFi.encryptionType(i));
      //json += ",\"hidden\":"+String(WiFi.isHidden(i)?"true":"false");
      json += "}<br>";
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
      WiFi.scanNetworks(true);
    }
  }
  json += "]</div>";
  //request->send(200, "text/html", json);
  return json;
}

void handleDoCommand(AsyncWebServerRequest *request){
  for(uint8_t i = 0; i< request->args(); i++){
    if(request->argName(i) == "command"){
      commandESP32(char(String(request->arg(i))[0]));
    }
  }
}

void handleWiFiSetup(AsyncWebServerRequest *request){
  String scanResult = scanWiFi();
  request->send(200, "text/html", connect_page1 + scanResult + connect_page2); //Send web page 
}

void handleDoConnect(AsyncWebServerRequest *request) {

  bool save = true; //temp: always true until setupStation is functional mid-program.
  bool ap_only = false;
  bool use_static = false;
  bool btSwitch = false;
  
  for(uint8_t i = 0; i < request->args(); i++){
    if(request->argName(i) == "ssid"){setSSID = String(request->arg(i)); Serial.print("SSID: "); Serial.println(setSSID);}
    if(request->argName(i) == "pw"){setPass = String(request->arg(i)); Serial.print("Password: "); Serial.println(setPass);}
    if(request->argName(i) == "static"){ staticIP = String(request->arg(i)); Serial.print("Static IP: "); Serial.println(staticIP);}
    if(request->argName(i) == "gateway"){ gateway = String(request->arg(i)); Serial.print("Gateway IP: "); Serial.println(gateway);}
    if(request->argName(i) == "subnet"){ subnetM = String(request->arg(i)); Serial.print("Subnet Mask: "); Serial.println(subnetM);  }
    if(request->argName(i) == "use_static"){ use_static = bool(request->arg(i)); Serial.print("Use Static IP: "); Serial.println(use_static);}
    if(request->argName(i) == "AP_ONLY"){ap_only = bool(request->arg(i)); Serial.print("AP Only: "); Serial.println(ap_only);}
    if(request->argName(i) == "btSwitch"){btSwitch = bool(request->arg(i)); Serial.print("Use Bluetooth: "); Serial.println(btSwitch);}
    //if(request->argName(i) == "save"){save = bool(request->arg(i)); Serial.println(save);}
  }
  delay(100);
  if (btSwitch == false) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device connects.");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    delay(100);
    if(save==true){
      saveWiFiLogin(ap_only,use_static,false);
    }
    //if((save==false)&&(ap_only==true)){
      //EEPROM.begin(512);
      //EEPROM.write(1,0);
      //EEPROM.commit();
      //EEPROM.end();
    //}
    if(setSSID.length() > 0) {
      if(use_static == true){
        setupStation(true);
      }
      else { 
        setupStation(false); 
      }
    }
    else {
      ESP.restart();
    }
  }
  else {
    commandESP32('b');
  }
  delay(100);
}
 
void handleUpdate(AsyncWebServerRequest *request) {
  //char* html = "<h4>Upload compiled sketch .bin file</h4><form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", update_page);
  request->send(response);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  //uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)){
      Update.printError(Serial);
    }
  }
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  if (final) {
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
      response->addHeader("Refresh", "20");  
      response->addHeader("Location", "/");
      request->send(response);
      delay(100);
      ESP.restart();
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

void checkInput()
{
  /*if (USE_BT == true)
  {
    if (SerialBT.available())
    {
      received = SerialBT.read();
      SerialBT.println(received);
      commandESP32(received);
      SerialBT.read(); //Flush endline for single char response
    }
  }*/
  if (USE_USB == true)
  {
    if (Serial.available())
    {
      received = Serial.read();
      Serial.println(received);
      commandESP32(received);
      Serial.read();
    }
  }
}

void wsHEGTask(void *p){
  while(globalWSClient != NULL && globalWSClient->status() == WS_CONNECTED){
    globalWSClient->text(output);
    vTaskDelay(75 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

//void evsHEGTask(void *p){
  //events.send(output,"event",millis());
//}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
 
  if(type == WS_EVT_CONNECT){
 
    Serial.println("Websocket client connection received");
    //commandESP32('t');
    globalWSClient = client;
    xTaskCreate(wsHEGTask,"HEG_ws",8196,NULL,2,NULL);
 
  } else if(type == WS_EVT_DISCONNECT){
 
    Serial.println("Websocket client connection finished");
    //commandESP32('f');
    globalWSClient = NULL;
 
  }
}

void setupWiFi(){

  //WiFi.mode(AP_STA_MODE);
  EEPROM.begin(512);
  //Serial.println(EEPROM.read(0));
  //Serial.println(EEPROM.readString(2));
  //Serial.println(EEPROM.readString(128));
  if ((defaultConnectWifi == true) || (EEPROM.read(1) == 1)){
  //ESP32 connects to your wifi -----------------------------------
    if(EEPROM.read(1) == 1){ //Read from flash memory
      setSSID = EEPROM.readString(2);
      setPass = EEPROM.readString(128);
      staticIP = EEPROM.readString(256);
      gateway = EEPROM.readString(320);
      subnetM = EEPROM.readString(384);
      //setupStation(setSSID,setPass);
    }
    else{
      setSSID = String(ssid);
      setPass = String(password);
    }
    setupStation(bool(EEPROM.read(255)));
  //----------------------------------------------------------------
  }
  else {
    if (WiFi.waitForConnectResult() != WL_CONNECTED){
      connectAP();
    }
  }
  EEPROM.end();
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it gat is: %u\n", client->lastId());
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    //client->send("hello!",NULL,millis(),1000);
  });
  
  //HTTP Basic authentication
  //events.setAuthentication("user", "pass");
  server.addHandler(&events);

  WiFi.scanNetworks(true);
  WiFi.scanDelete();
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", MAIN_page); //Send web page 
  });      //This is the default display page
  server.on("/hegvid",HTTP_GET,[](AsyncWebServerRequest *request) {
    request->send_P(200,"text/html",video_page);
  });
  server.on("/graph",HTTP_GET,[](AsyncWebServerRequest *request){
    request->send_P(200,"text/html",graph_page);
  });
  server.on("/sc",HTTP_GET,[](AsyncWebServerRequest *request){
    request->send_P(200,"text/html", sc_page);
  });
  server.on("/stream",HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", ws_page);
    delay(1000);
    deviceConnected = true;
  });
  server.on("/listen",HTTP_GET, [](AsyncWebServerRequest *request){
    //xTaskCreate(evsHEGTask,"evsHEGTask",8196,NULL,2,NULL);
    //commandESP32('t');
    request->send_P(200,"text/html", event_page);
  });
  server.on("/connect",HTTP_GET,[](AsyncWebServerRequest *request){
    handleWiFiSetup(request);
  });
  server.on("/doConnect",HTTP_POST, [](AsyncWebServerRequest *request){
    handleDoConnect(request);
  });
  //First request will return 0 results unless you start scan from somewhere else (loop/setup)
  //Do not request more often than 3-5 seconds
  server.on("/doScan", HTTP_POST, [](AsyncWebServerRequest *request){
    String temp = scanWiFi();
  });
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200,"text/html",scanWiFi());
  });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    handleUpdate(request);
  });
  server.on("/doUpdate", HTTP_POST,
    [&](AsyncWebServerRequest *request) {
      // the request handler is triggered after the upload has finished... 
      // create the response, add header, and send response
      AsyncWebServerResponse *response = request->beginResponse((Update.hasError())?500:200, "text/plain", (Update.hasError())?"FAIL":"OK");
      response->addHeader("Connection", "close");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
      },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});

  //Text-based commands. Send char corresponding to known commands.
  server.on("/command",HTTP_POST,[](AsyncWebServerRequest *request){
    handleDoCommand(request);
  });
  
  //POST-based commands
  server.on("/startHEG",HTTP_POST,[](AsyncWebServerRequest *request){
    commandESP32('t');
  });
  server.on("/stopHEG",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('f');
  });
  server.on("/restart",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('R');
  });
  server.on("/l",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('l');
  });
  server.on("/c",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('c');
  });
  server.on("/r",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('r');
  });
  server.on("/n",HTTP_POST, [](AsyncWebServerRequest *request){
    commandESP32('n');
  });
 
  server.begin();                  //Start server
  Serial.println("HTTP server started");

  Update.onProgress(printProgress);

  
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}
