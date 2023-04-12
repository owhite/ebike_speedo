#include <CAN.h>

#define CAN_ID_SPEED     0x2A00000
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

  if (packetSize == CAN_PACKET_SIZE && CAN.packetId() == CAN_ID_SPEED) {
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

