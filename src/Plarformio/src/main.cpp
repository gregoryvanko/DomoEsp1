#include <Arduino.h>
#include "config.h"
#include "WifiAuto.h"
#include "Button.h"

// Definition du wifi
WifiAuto wifi;                       // bouton BOOT (GPIO 0), AP "ESP32-WifiAuto"
// WifiAuto wifi(4, "MonESP32");     // ou une autre GPIO / un autre nom d'AP

// Definition des bouttons
Button bouton1(13, false, 50);
Button bouton2(14, false, 50);


// Deniere mesure (ms)
int32_t lastMesure = 0;


// Action lors d'un chengement d'état se réalise sur un bouton
void ActionOnButtonChange(uint8_t PinNumer, bool pinvalue) {
  // Print pin value
  Serial.print("pin ");
  Serial.print(PinNumer);
  Serial.print(" : ");
  Serial.println(pinvalue);

  // Send MQTT message
  //sendMQTT(mqtt_topicPin + String(PinNumer), String(pinvalue));
}


void setup() {
  Serial.begin(CONFIG_BAUDRATE);
  delay(1000);

  // Start WifiAuto
  wifi.begin();

  // Start buttons
  bouton1.begin();
  bouton2.begin();

  // Definition des actions à réaliser lors d'un changement d'état sur les boutons
  bouton1.onLow([]()  { ActionOnButtonChange(1, 0);  });
  bouton1.onHigh([]() { ActionOnButtonChange(1, 1); });

  bouton2.onLow([]()  { ActionOnButtonChange(2, 0);  });
  bouton2.onHigh([]() { ActionOnButtonChange(2, 1); });
}

void loop() {
  // Update WifiAuto
  wifi.update();

  // Update buttons
  bouton1.update();
  bouton2.update();


  // Mesure toutes les CONFIG_MESURE_INTERVAL ms
  int32_t now = millis();
  if (now - lastMesure >= CONFIG_MESURE_INTERVAL){
    lastMesure = now;
    //Serial.println("Hello, World!");
  }
}
