#include <Arduino.h>
#include <ArduinoJson.h> // V7.4.2
//#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <MQTT.h> // V2.5.2
#include <OneWire.h> // V2.3.8
#include <DallasTemperature.h> // V4.0.6
#include <ESPAsyncWebServer.h>// V3.9.6
#include <AsyncTCP.h> // V3.4.10
#include "LittleFS.h"
#include <Adafruit_NeoPixel.h> // V1.15.4
#include <ESPmDNS.h>

// board : ESP32 V3.3.7

//Variables to save values from HTML form
String ssid;
String pass;
String mqttserv;
String mqttuser;
String mqttpass;
//****** test *************
// String ssid= "blacknet-IOT";
// String pass= "gregoryvk99iot";
// String mqttserv= "192.168.40.40";
// String mqttuser= "gregory";
// String mqttpass= "gregory";


// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "mqttserv";
const char* PARAM_INPUT_4 = "mqttuser";
const char* PARAM_INPUT_5 = "mqttpass";

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* mqttservPath = "/mqttserv.txt";
const char* mqttuserPath = "/mqttuser.txt";
const char* mqttpassPath = "/mqttpass.txt";

// MQTT Topic
const String mqtt_topic = "Domo1";
const String mqtt_topicAll = "All";
const String mqtt_topicTemp1 = "Temp1";
const String mqtt_topicPin = "Pin";
const String mqtt_get = "Get";

// GPIO input count
const uint8_t GpioInputCount = 8; 
// GPIO input pin
const uint8_t GpioInput[GpioInputCount] = {2, 11, 10, 1, 0, 7, 6, 5};
// GPIO input status
volatile bool statusInput[GpioInputCount];
// GPIO input lastTime
volatile unsigned long lastTimeInput[GpioInputCount];
// Button est il en transition
volatile bool ButtonTransition[GpioInputCount];

// GPIO where the DS18B20 is connected to 
const int oneWireBus = 3;
// Temperature status
volatile float statustemp1 = 0;
// Temperature lastTime
volatile unsigned long lsastTimeTemp1 = 0;
// Timer (s) pour envoyer la temperature
const long timerforsendtemp = 60000; 

// Internal led
const uint8_t LED_PIN = 8;
const uint8_t NUM_LEDS = 1;
// Create rbg pixel
Adafruit_NeoPixel rgbLed(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
struct RGB {
    uint8_t r, g, b;
};
// Custom color
const RGB COLOR_OFF   = {0, 0, 0};
const RGB COLOR_Blue = {0, 0, 255}; 
const RGB COLOR_Red = {255, 0, 0}; 
const RGB COLOR_Green = {0, 255, 0}; 
const RGB COLOR_Yellow = {255, 255, 0}; 

// Dedouncing delay
const unsigned long DEBOUNCE_DELAY = 500;
// Timer variables for wifi connection
unsigned long wifiConnPreviousMillis = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer localServer(80);

// Create websocket sur /WS
AsyncWebSocket ws("/ws");

// Define MQTT
MQTTClient clientMqtt;
WiFiClient net;

// Define DS18B20
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Wifi connection satatus
bool isWifiConnectToRouteur = false;

// mqtt connection satatus
bool isMqttConnectToServer = false;

// ws connection satatus
bool isWSConnected = false;

//********************************************
// Connect MQTT
void mqttconnect() {
  // Check wifi connection
  Serial.print("checking wifi...");
  unsigned long currentMillis = millis();
  wifiConnPreviousMillis = currentMillis;
  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - wifiConnPreviousMillis >= 5000) {
      Serial.println("\nFailed to connect to wifi");
      deleteFile(LittleFS, ssidPath);
      deleteFile(LittleFS, passPath);
      deleteFile(LittleFS, mqttservPath);
      deleteFile(LittleFS, mqttuserPath);
      deleteFile(LittleFS, mqttpassPath);

      Serial.println("Restart ESP");
      delay(3000);
      ESP.restart();
    }
    delay(1000);
    Serial.print(".");
  }

  // Connect to mqtt
  Serial.print("\nMQTT connecting...");
  while (!clientMqtt.connect("Domo", mqttuser.c_str(), mqttpass.c_str())) {
    currentMillis = millis();
    if (currentMillis - wifiConnPreviousMillis >= 5000) {
      Serial.println("\nFailed to connect to mqtt");
      deleteFile(LittleFS, ssidPath);
      deleteFile(LittleFS, passPath);
      deleteFile(LittleFS, mqttservPath);
      deleteFile(LittleFS, mqttuserPath);
      deleteFile(LittleFS, mqttpassPath);

      Serial.println("Restart ESP");
      delay(3000);
      ESP.restart();
    }
    Serial.print(".");
    delay(1000);
    isMqttConnectToServer = false;
  }

  Serial.println("\nMQTT connected!");

  // set isMqttConnectToServer to true
  isMqttConnectToServer = true;

  // Subsribe to mqtt topic
  clientMqtt.subscribe(mqtt_topic + "/" + mqtt_get);  
}

// Receive MQTT message
void messageReceived(String &topic, String &payload) {
  // Print incuming
  Serial.println("incoming: " + topic + " - " + payload);

  if (topic == mqtt_topic + "/" + mqtt_get){
    if (payload == mqtt_topicAll){
      // Send All Pin
      SendAllPin();
    } else {
      Serial.println("payload inconnu: " + payload);
    }
  } else {
    Serial.println("topic inconnu: " + topic);
  }

  // Note: Do not use the clientMqtt in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `cliclientMqttent.loop()`.
}

// Send message to MQTT
void sendMQTT(String topic, String message){
  if (isMqttConnectToServer){
    clientMqtt.publish(mqtt_topic + "/" + topic, message);
  } else {
    Serial.println("mqtt client not connected");
  }
}

// Send message to ws
void sendWS(String topic, String message){
  if (isWSConnected){
    ws.textAll("{\"channel\":\"" + topic + "\",\"value\":\"" + message + "\"}");
  } else {
    Serial.println("ws not connected");
  }
}

void SendAllPin(){
  for (int thisPin = 0; thisPin < GpioInputCount; thisPin++) {
    // Print pin value
    Serial.print("pin ");
    Serial.print(thisPin + 1);
    Serial.print(" : ");
    Serial.println(statusInput[thisPin]);

    // Send MQTT message
    sendMQTT(mqtt_topicPin + String(thisPin + 1), String(statusInput[thisPin]));

    // Send to WS isWSConnected
    sendWS(mqtt_topicPin + String(thisPin + 1), String(statusInput[thisPin]));
  }
}

// Action a faire lorsque le statu d'une pin change
void actionPinChange(uint8_t PinNumer, bool pin) {
  // Print pin value
  Serial.print("pin ");
  Serial.print(PinNumer + 1);
  Serial.print(" : ");
  Serial.println(pin);

  // Send MQTT message
  sendMQTT(mqtt_topicPin + String(PinNumer + 1), String(pin));

  // Send to WS isWSConnected
  sendWS(mqtt_topicPin + String(PinNumer + 1), String(pin));
}

// Analyse change on pinout
void analysePin(unsigned long now, uint8_t PinNumer, volatile bool & lastButtonState, volatile unsigned long & lastTimeIn, volatile bool & ButtonInTransition){
  // Read pin
  bool pin = digitalRead(GpioInput[PinNumer]);

  // Si le status est différent du précédent
  if (pin != lastButtonState){
    if (ButtonInTransition == false){
      lastTimeIn = now;
      ButtonInTransition = true;
    }
  } else {
    ButtonInTransition = false;
  }
  if (now - lastTimeIn > DEBOUNCE_DELAY) {
    if (pin != lastButtonState){
      lastButtonState = pin;
      ButtonInTransition = false;
      actionPinChange(PinNumer, pin);
    } 
  }
    
  // reste listtimin after reste du now
  if (now < lastTimeIn){
    lastTimeIn=0;    
  }
}

// Analyse temperature
void analyseTemp1(unsigned long now){
  if (now - lsastTimeTemp1 > timerforsendtemp){
    lsastTimeTemp1 = now;

    // Read sensor value
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);

    // Print value
    Serial.print("temp1 : ");
    Serial.print(temperatureC);
    Serial.println("ºC");

    // Send MQTT message
    sendMQTT(mqtt_topicTemp1, String(temperatureC));

    // Send to WS isWSConnected
    sendWS(mqtt_topicTemp1, String(temperatureC));
    
  } else if (now < lsastTimeTemp1){
    lsastTimeTemp1=0;    
  }
}

// Set color of rgb led
void setColor(const RGB& color, uint8_t brightness = 100) {
    uint16_t scale = (uint16_t)brightness * 255 / 100;
    uint8_t r = (uint8_t)(((uint16_t)color.r * scale) / 255);
    uint8_t g = (uint8_t)(((uint16_t)color.g * scale) / 255);
    uint8_t b = (uint8_t)(((uint16_t)color.b * scale) / 255);
    rgbLed.setPixelColor(0, rgbLed.Color(g, r, b));
    rgbLed.show();
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Lsit files
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

// Delete file
void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}

// Check if SSID is define
bool CheckifSSIDisDefine() {
  if(ssid==""){
    Serial.println("Undefined SSID");
    return false;
  } else {
    return true;
  }
}

// WS handle socket message
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  String message = "";
  for (size_t i = 0; i< len; i++){
    message += (char)data[i];
  }
  Serial.print("WS message: ");
  Serial.println(message.c_str());
  // ToDo
}

// WS on event
void WSonEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      SendAllPin();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

//******************************
// setup
void setup() {
  // switch off internal Led
  rgbLed.begin(); 
  rgbLed.clear();
  setColor(COLOR_OFF, 100);

  // setup serial
  Serial.begin(115200);
  delay(3000);
  Serial.println("\nsetup : start");

  // Setup pinout
  Serial.println("setup : pinout");
  for (int thisPin = 0; thisPin < GpioInputCount; thisPin++) {
    pinMode(GpioInput[thisPin], INPUT_PULLUP);
  }

  // Pin status
  for (int thisPin = 0; thisPin < GpioInputCount; thisPin++) {
    // Get pin status
    statusInput[thisPin]= digitalRead(GpioInput[thisPin]);
    // Set lastTimeInput array to 0
    lastTimeInput[thisPin]= 0;
    // Set ButtonTransition array to 0
    ButtonTransition[thisPin]= false;
  }

  // setup the DS18B20 sensor
  Serial.println("setup : DS18B20");
  sensors.begin();

  // setup LittleFs
  initLittleFS();

  // Load values saved in LittleFS
  if(!CheckifSSIDisDefine()) {
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    mqttserv = readFile(LittleFS, mqttservPath);
    mqttuser = readFile (LittleFS, mqttuserPath);
    mqttpass = readFile (LittleFS, mqttpassPath);
  }
  

  // Print connection data
  Serial.print("ssid: ");
  Serial.println(ssid);
  Serial.print("pass: ");
  Serial.println(pass);
  Serial.print("mqttserv: ");
  Serial.println(mqttserv);
  Serial.print("mqttuser: ");
  Serial.println(mqttuser);
  Serial.print("mqttpass: ");
  Serial.println(mqttpass);

  

  // Setup wifi
  if(CheckifSSIDisDefine()) {
    // connect to wifi router
    Serial.println("Connect to wifi router...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long currentMillis = millis();
    wifiConnPreviousMillis = currentMillis;

    while(WiFi.status() != WL_CONNECTED) {
      currentMillis = millis();
      if (currentMillis - wifiConnPreviousMillis >= 5000) {
        // Failed to connect to the router
        Serial.println("Failed to connect to router. Restart ESP");
        deleteFile(LittleFS, ssidPath);
        deleteFile(LittleFS, passPath);
        deleteFile(LittleFS, mqttservPath);
        deleteFile(LittleFS, mqttuserPath);
        deleteFile(LittleFS, mqttpassPath);

        Serial.println("Restart ESP");
        delay(3000);
        ESP.restart();
      }
    }

    // Print local ip
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Route for root / web page
    localServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
    });

    localServer.serveStatic("/", LittleFS, "/");

    localServer.on("/action", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      Serial.println("POST action");
      Serial.print("Data: ");
      Serial.println(String((char *)data));
      JsonDocument doc;
      deserializeJson(doc, data);
      const String Action = doc["A"];
      const String Data = doc["D"];

      if(Action == "reset"){
        if (Data == "true"){
          deleteFile(LittleFS, ssidPath);
          deleteFile(LittleFS, passPath);
          deleteFile(LittleFS, mqttservPath);
          deleteFile(LittleFS, mqttuserPath);
          deleteFile(LittleFS, mqttpassPath);

          Serial.println("Restart ESP");
          delay(3000);
          ESP.restart();
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart");
    });

    // Socket event
    ws.onEvent(WSonEvent);
    localServer.addHandler(&ws);
    isWSConnected = true;
    
    // Begin local server
    localServer.begin();

    // Set led color to bleu
    setColor(COLOR_Green, 100);

    // set isWifiConnectToRouteur to true
    isWifiConnectToRouteur = true;
  }
  else {
    // set isWifiConnectToRouteur to false
    isWifiConnectToRouteur = false;

    // set wifi in Access Point
    Serial.println("Setting wifi in Access Point");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI", NULL);

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP()); 

    // Root URL
    localServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    localServer.serveStatic("/", LittleFS, "/");
    
    localServer.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      Serial.println("POST from AP mode");
      int params = request->params();
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST mqttserv value
          if (p->name() == PARAM_INPUT_3) {
            mqttserv = p->value().c_str();
            Serial.print("mqttserv Address set to: ");
            Serial.println(mqttserv);
            // Write file to save value
            writeFile(LittleFS, mqttservPath, mqttserv.c_str());
          }
          // HTTP POST mqttpass value
          if (p->name() == PARAM_INPUT_4) {
            mqttuser = p->value().c_str();
            Serial.print("mqttuser set to: ");
            Serial.println(mqttuser);
            // Write file to save value
            writeFile(LittleFS, mqttuserPath, mqttuser.c_str());
          }
          // HTTP POST mqttpass value
          if (p->name() == PARAM_INPUT_5) {
            mqttpass = p->value().c_str();
            Serial.print("mqttpass set to: ");
            Serial.println(mqttpass);
            // Write file to save value
            writeFile(LittleFS, mqttpassPath, mqttpass.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart");

      Serial.println("Restart ESP");
      delay(3000);
      ESP.restart();
    });

    localServer.begin();
    
    // Set led color to bleu
    setColor(COLOR_Blue, 100);
  }

  // setup MQTT
  clientMqtt.begin(mqttserv.c_str(), net);
  clientMqtt.onMessage(messageReceived);

  // Connect to mqtt
  if (isWifiConnectToRouteur){
    mqttconnect();
    // Send All Pin to mqtt if connected to mqtt server
    if (isMqttConnectToServer){
      SendAllPin();
    }
  }

  // Liste all file
  listDir(LittleFS, "/", 3);

  // setup ESPmDNS
  Serial.println("setup : ESPmDNS");
  if (! MDNS.begin("domo1")){
    Serial.println("Error to setup ESPmDNS");
  } else {
    MDNS.addService("http", "tcp", 80);
  }
  
  // Send serial done
  Serial.println("setup: done");
}

//******************************
// loop
void loop() {
  // Si le wifi est connecté à un routeur
  if (isWifiConnectToRouteur){
    // Validation of mqtt connection
    if (isMqttConnectToServer == false) {
      mqttconnect();
    } else {
      clientMqtt.loop();
      delay(10);
    }
    if (isWSConnected){
      ws.cleanupClients();
    }
  }
  
  // get now
  long now = millis();

  // Analyse de la temperature
  analyseTemp1(now);

  // Analyse input pin
  for (int thisPin = 0; thisPin < GpioInputCount; thisPin++) {
    analysePin(now, thisPin, statusInput[thisPin], lastTimeInput[thisPin], ButtonTransition[thisPin]);
  }
}
