#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 

// #define CAN_ID_SPEED     0x2A00000
#define CAN_ID_SPEED     0x2A0
#define CAN_PACKET_SIZE  8

boolean blinkOn = false;
uint32_t blinkDelta = 0;
uint32_t blinkInterval = 100; 
uint32_t blinkNow;

uint32_t val = 0;


void setup() {
  Serial.begin(9600);

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false); // turn off STANDBY
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true); // turn on booster
  
  pinMode(LED_BUILTIN, OUTPUT);

  if (!CAN.begin(500000)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  alphaLED.begin(0x70);
  alphaLED.setBrightness(0);
  alphaLED.clear();
  alphaLED.writeDigitAscii(1, 'O');
  alphaLED.writeDigitAscii(2, 'F');
  alphaLED.writeDigitAscii(3, 'F');
  alphaLED.writeDisplay();

  delay(1000); 
}

void loop() {

  // non-blocking blink
  blinkNow = millis();
  if ((blinkNow - blinkDelta) > blinkInterval) {
    blinkOn = !blinkOn;
    digitalWrite(LED_BUILTIN, blinkOn);
  }

  int packetSize = CAN.parsePacket();

  if (packetSize == CAN_PACKET_SIZE && (CAN.packetId() >> 16) == CAN_ID_SPEED) {

    int n = CAN.packetId();
    n = n >> 16;
    displayNum( n ); 
    alphaLED.writeDisplay();

    Serial.print("Received speed: ");
    if (CAN.packetExtended()) {
      Serial.print("extended ");
    }

    if (CAN.packetRtr()) {
      Serial.print("RTR "); // contained no data
    }

    if (CAN.packetRtr()) {
      Serial.print(" and requested length ");
      Serial.println(CAN.packetDlc());
    }
    else {
      // only print packet data for non-RTR packets
      while (CAN.available()) {
        Serial.print((char)CAN.read());
      }
      Serial.println();
    }

    Serial.println();
  }
}

// right justifies digit on to the alpha display
//  this uses the hideous arduino String() function
void displayNum(int n) {
  String str = String(n);
  if (str.length() < 4) {
    for(int i=0;i < str.length(); i++) {
      Serial.print(str.charAt(i));
      alphaLED.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
  Serial.println();
}
