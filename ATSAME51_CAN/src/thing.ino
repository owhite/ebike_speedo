#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
// from: MESC_Firmware/MESC_RTOS/Tasks/can_ids.h
#include <can_ids.h> 
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 

#define CAN_PACKET_SIZE  8
#define CAN_DEBUG 0

#define CAN_ELEMENTS 3
uint16_t canSet[] = {CAN_ID_SPEED, CAN_ID_MOTOR_CURRENT, CAN_ID_TEMP_MOT_MOS1};
uint8_t prefixes[] = {'M', 'A', 'T'}; // maintain the order with above values
float canValues[CAN_ELEMENTS] = {};
uint8_t canBuf[8] = {};


union {
  uint8_t b[4];
  float f;
} canData;

boolean blinkOn = false;
uint32_t blinkDelta = 0;
uint32_t blinkInterval = 100; 
uint32_t blinkNow;

uint16_t packetId;

int ifInSet(uint16_t i) {
  for (int x = 0; x < CAN_ELEMENTS; x++)  {
    if (i == canSet[x]) {
      return(x);
      break;
    }
  }
  return( -1 );
}

uint16_t extract_id(uint32_t ext_id) {
  return (ext_id >> 16);
}

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

  // register CAN callback
  CAN.onReceive(onReceive);

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

  for (int i = 0; i<CAN_ELEMENTS; i++) {
    Serial.print(char(prefixes[i]));
    Serial.print(" :: ");
    Serial.println(canValues[i]);
  }
  delay(800);
}

void onReceive(int packetSize) {
  // received a packet

  packetId = extract_id(CAN.packetId());

  if ( CAN_DEBUG ) {

    Serial.print("Received ");

    if (CAN.packetExtended()) {
      Serial.print("extended ");
    }

    if (CAN.packetRtr()) {
      // Remote transmission request, packet contains no data
      Serial.print("RTR ");
    }

    Serial.print("packet with id 0x");
    Serial.print(CAN.packetId(), HEX);

    if (CAN.packetRtr()) {
      Serial.print(" and requested length ");
      Serial.println(CAN.packetDlc());
    }
    else {
      Serial.print(" and length ");
      Serial.println(packetSize);

      // only print packet data for non-RTR packets
      while (CAN.available()) {
	Serial.print((char)CAN.read());
      }
      Serial.println();
    }
    if (CAN.packetExtended()) {
      Serial.print("EXTENDED");
    }
  }

  if (packetSize == CAN_PACKET_SIZE) {
    int val = ifInSet(packetId); // shitty filter
    if (val != -1) {
      // Serial.print("PACKET :: ");
      // Serial.print(packetId, HEX);
      // Serial.print(":: ");
      for (int i = 0; i < packetSize; i++) {
	canBuf[i] = CAN.read();
      }
      canData.b[0]=canBuf[4];
      canData.b[1]=canBuf[5];
      canData.b[2]=canBuf[6];
      canData.b[3]=canBuf[7];
      canValues[val] = canData.f;
      // Serial.println(canData.f);
    }
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
