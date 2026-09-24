#include <Arduino.h>
#include "config.h"
#include "WifiAuto.h"
#include "Button.h"
#include "SensorTemperature.h"

// Definition du wifi
WifiAuto wifi(CONFIG_PIN_BUTTON_BOOT, CONFIG_WIFI_ESP32SSID, CONFIG_MQTT_TOPIC_STATUS);

// Definition du capteur de temperature
SensorTemperature SensorTemperature1(CONFIG_PIN_TEMPERATURE);

// Definition des bouttons
Button boutons[] = {
  Button(CONFIG_PIN_BUTTON1, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON2, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON3, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON4, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON5, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON6, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON7, false, CONFIG_DEBOUNCE_MS),
  Button(CONFIG_PIN_BUTTON8, false, CONFIG_DEBOUNCE_MS),
};
constexpr size_t NB_BOUTONS = sizeof(boutons) / sizeof(boutons[0]);

// Action lors d'un chengement d'état se réalise sur un bouton
void ActionOnButtonChange(uint8_t PinNumber, bool pinvalue) {
  // Print pin value
  Serial.println("pin " + String(PinNumber) + " : " + String(pinvalue));

  // Send MQTT message
  wifi.mqttPublish(String(CONFIG_MQTT_TOPIC_PIN) + String(PinNumber), String(pinvalue));
}

// Publie l'état de tous les boutons
void SendAllStatus() {
  for (size_t i = 0; i < NB_BOUTONS; i++) {
    ActionOnButtonChange(i + 1, boutons[i].isHigh());
  }
}

// Definition du callback pour la reception des messages MQTT
void onMqttMessage(const String& topic, const String& payload) {
  Serial.println("Recu sur " + topic + " : " + payload);
  if (topic == CONFIG_MQTT_TOPIC_GET){
    if (payload == CONFIG_MQTT_PAYLOAD_ALL){
      // Send All buttons status
      SendAllStatus();
    } else {
      Serial.println("payload inconnu: " + payload);
    }
  } else {
    Serial.println("topic inconnu: " + topic);
  }
}

// Deniere mesure (ms)
uint32_t lastMesure = 0;

// Setup and loop
void setup() {
  // Start Serial
  Serial.begin(CONFIG_BAUDRATE);
  delay(1000);

  // Definition du callback pour la reception des messages MQTT
  wifi.setMqttMessageCallback(onMqttMessage);

  // Definition du callback pour la connexion au broker MQTT
  wifi.onMqttConnected([]() {
    wifi.mqttSubscribe(CONFIG_MQTT_TOPIC_GET);
  });

  // Start WifiAuto
  wifi.begin();

  // Start buttons et definition des actions à réaliser lors d'un changement d'état
  for (size_t i = 0; i < NB_BOUTONS; i++) {
    uint8_t numero = i + 1;
    boutons[i].begin();
    boutons[i].onLow([numero]()  { ActionOnButtonChange(numero, 0); });
    boutons[i].onHigh([numero]() { ActionOnButtonChange(numero, 1); });
  }

  // Start temperature sensor
  SensorTemperature1.begin();
}

void loop() {
  // Update WifiAuto
  wifi.update();

  // Update buttons
  for (Button& b : boutons) b.update();

  // Mesure toutes les CONFIG_MESURE_INTERVAL ms
  uint32_t now = millis();
  if (now - lastMesure >= CONFIG_MESURE_INTERVAL){
    // Update last mesure time
    lastMesure = now;
    // Read temperature
    float Temp = SensorTemperature1.read();
    // Send MQTT message
    wifi.mqttPublish(CONFIG_MQTT_TOPIC_TEMPERATURE, String(Temp));
  }
}
