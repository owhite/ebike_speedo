#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

#define BAUDRATE 115200

#define LED_PIN 13

boolean blinkOn = false;
uint32_t blinkDelta = 0;
uint32_t blinkInterval = 300; 
uint32_t blinkNow;

void setup() {
  Serial.begin(BAUDRATE);
  Serial1.begin(BAUDRATE);
  SerialBT.begin("MP2 LINK");

  pinMode(LED_PIN, OUTPUT);
}

void loop() {

  char c1;
  char c2;
  char c3;

 
  blinkNow = millis();
  if ((blinkNow - blinkDelta) > blinkInterval) {
    blinkOn = !blinkOn;
    digitalWrite(LED_PIN, blinkOn);
  }

  while (Serial.available()) {
    c1 = Serial.read();
    Serial.write(c1); // always relay this serial

    Serial1.write(c1);
    SerialBT.write(c1);
  }

  while (Serial1.available()){
    c2 = Serial1.read();
    Serial.write(c2);
    SerialBT.write(c2);
  }

  while (SerialBT.available()){
    c3 = SerialBT.read();
    Serial.write(c3);
    Serial1.write(c3);
  }

}

