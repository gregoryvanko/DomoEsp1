#include <Arduino.h>
#include "config.h"
#include "Button.h"

//definition des bouttons
Button bouton1(13, false, 50);
Button bouton2(14, false, 50);

// Action des bouttons


// Deniere mesure (ms)
int32_t lastMesure = 0;


void setup() {
  Serial.begin(CONFIG_BAUDRATE);
  delay(1000);

  bouton1.begin();
  bouton2.begin();


  bouton1.onLow([]()  { Serial.println("Bouton 1 LOW");  });
  bouton1.onHigh([]() { Serial.println("Bouton 1 HIGH"); });

  bouton2.onLow([]()  { Serial.println("Bouton 2 LOW");  });
  bouton2.onHigh([]() { Serial.println("Bouton 2 HIGH"); });
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
