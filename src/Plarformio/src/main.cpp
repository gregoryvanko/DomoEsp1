#include <Arduino.h>
#include "config.h"
#include "Button.h"

//definition des bouttons
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

  bouton1.begin();
  bouton2.begin();


  bouton1.onLow([]()  { ActionOnButtonChange(1, 0);  });
  bouton1.onHigh([]() { ActionOnButtonChange(1, 1); });

  bouton2.onLow([]()  { ActionOnButtonChange(2, 0);  });
  bouton2.onHigh([]() { ActionOnButtonChange(2, 1); });
}

void loop() {

  bouton1.update();
  bouton2.update();


  // Initialisation de la mesure du temps
  //int32_t now = millis();
  //if (now - lastMesure >= CONFIG_MESURE_INTERVAL){
  //  // Actualisation de la mesure du temps
  //  lastMesure = now;
  //  Serial.println("Hello, World!");
  //}
}
