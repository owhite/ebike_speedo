#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <can_ids.h> // from: MESC_Firmware/MESC_RTOS/Tasks/can_ids.h
#include <Adafruit_LEDBackpack.h>
#include "debug.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 

#define BTN_PIN1       12
#define BTN_PIN2       13
#define KILLSWITCH_PIN 17

#define CAN_PACKET_SIZE  8
#define CAN_DEBUG        0

/////////
// bike variables
#define POLEPAIRS    7
#define CIRCUMFERENCE 219.5 // much easier to circumerence measure with tape
#define GEAR_RATIO 9.82
#define CM_IN_MILE 160900
#define MINVOLTAGE 58
#define MAXVOLTAGE 75 
#define MAXTEMP 70
float EHZ_scale = (CIRCUMFERENCE * 60 * 60) / (160900 * GEAR_RATIO * POLEPAIRS);

/////////
// CAN variables
unsigned long canReceiveTime;
unsigned long canInterval = 2000;
boolean receivedPacket = true;
const unsigned long canlInterval = 600;
// permitted values
uint16_t canSet[] = {CAN_ID_SPEED, CAN_ID_BUS_VOLT_CURR, CAN_ID_TEMP_MOT_MOS1};
const uint8_t canElements = 3;
uint8_t canBuf[8] = {};
uint16_t packetId;

union {
  uint8_t b[4];
  float f;
} canData1;

union {
  uint8_t b[4];
  float f;
} canData2;

/////////
// reporting things
unsigned long currentTime;
unsigned long reportLastTime;
unsigned long reportInterval = 400;
uint8_t prefixes[] = {'M', 'E', 'B', 'A', 'T'}; // maintain the order with canSet[]. 'M' is SPEED, etc
const uint8_t userRequestMax = 5; 
float reportValues[userRequestMax] = {};
uint8_t userRequest = 0; // what the user wants
uint8_t reportInc = 0; // cycles through things to display
uint8_t reportMax = 4; 
boolean reportCAN = false;   // set when exceeded timeout
boolean reportError = true;  // set when CAN says there's an error
boolean reportTemp  = true;  // set when over heating

///////// NOT USED
// blink variables
boolean blinkOn = false;
uint32_t lastBlinkTime = 0;
const uint32_t blinkInterval = 100; 
uint32_t blinkNow;

/////////
// button reading timers
const int SHORT_PRESS_TIME = 600;
const int LONG_PRESS_TIME  = 600;
int lastState1 = LOW;
int lastState2 = LOW;
int currentState1;
int currentState2;
unsigned long pressedTime1  = 0;
unsigned long pressedTime2  = 0;
unsigned long releasedTime1 = 0;
unsigned long releasedTime2 = 0;
// control if LED is bright or dim
bool brightnessToggle = true;

void setup() {
  Serial.begin(9600);

  pinMode(BTN_PIN1, INPUT);
  pinMode(BTN_PIN2, INPUT);
  pinMode(PIN_CAN_STANDBY, OUTPUT);
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false); // standby off
  digitalWrite(PIN_CAN_BOOSTEN, true);  // booster on
  
  if (!CAN.begin(500000)) {
    DEBUG_PRINTLN("Starting CAN failed!");
    while (1);
  }

  CAN.onReceive(onReceive); // register CAN callback

  alphaLED.begin(0x70);
  alphaLED.setBrightness(brightnessToggle * 14);
  alphaLED.clear();
  alphaLED.writeDigitAscii(1, 'O');
  alphaLED.writeDigitAscii(2, 'F');
  alphaLED.writeDigitAscii(3, 'F');
  alphaLED.writeDisplay();

  pressedTime1 = millis();
  pressedTime2 = millis();

  zipDisplay(0xBFFF); 
  zipDisplay(0xBFFF); 
  zipDisplay(0xBFFF); 
}

void loop() {
  canStatus();
  tempStatus();
  errorStatus();
  // button1Status();
  button2Status();

  switch (reportInc) {
  case 0: // show what the user wants
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, prefixes[userRequest]);
    displayNum( int( reportValues[userRequest] ) ); 
    alphaLED.writeDisplay();
    break;
  case 1: // show CAN status
    if (reportCAN) {
      alphaLED.clear();
      alphaLED.writeDigitAscii(1, 'C');
      alphaLED.writeDigitAscii(2, 'A');
      alphaLED.writeDigitAscii(3, 'N');
      alphaLED.writeDisplay();
      // DEBUG_PRINTLN("no CAN");
    }
    break;
  case 2: // show error status
    if (reportError) {
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, 'E');
      displayNum( int( reportValues[userRequest] ) ); 
    }
    break;
  case 3: // show temp status
    if (reportTemp) {
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, 'T');
      alphaLED.writeDigitAscii(1, 'E');
      alphaLED.writeDigitAscii(2, 'M');
      alphaLED.writeDigitAscii(3, 'P');
      alphaLED.writeDisplay();
    }
    break;
  default:
    break;
  }

  currentTime = millis();
  if ((currentTime - reportLastTime) > reportInterval) {  // went over time
    reportLastTime = currentTime;
    bumpReportInc();
  }
}

void bumpReportInc() {
  reportInc++;
  if (reportInc > reportMax) { reportInc = 0; }
}

void canStatus() {
  if (receivedPacket) {
    canReceiveTime = millis();
    receivedPacket = false;
    reportCAN = false;
  }

  currentTime = millis();
  if ((currentTime - canReceiveTime ) > canInterval) {  
    canReceiveTime = currentTime;
    reportCAN = true; // went over time
  }
}

void tempStatus() { // dunt work
  reportTemp = false;
}

void errorStatus() { // dunt work
  reportError = false;
}

void blinkLED() {
  // non-blocking blink
  currentTime= millis();
  if ((currentTime - lastBlinkTime) > blinkInterval) {
    lastBlinkTime = currentTime;
    blinkOn = !blinkOn;
    digitalWrite(LED_BUILTIN, blinkOn);
  }
}

void onReceive(int packetSize) {
  receivedPacket = true;
  // received a packet
  packetId = extract_id(CAN.packetId());
  if (packetSize == CAN_PACKET_SIZE) {
    int val = ifInSet(packetId); // best way to filter?
    if (val != -1) {

      DEBUG_PRINT("PACKET :: ");
      DEBUG_PRINT(packetId);
      DEBUG_PRINT(" :: VAL :: ");
      DEBUG_PRINT(val);
      for (int i = 0; i < packetSize; i++) {
	canBuf[i] = CAN.read();
      }
      canData1.b[0]=canBuf[0];
      canData1.b[1]=canBuf[1];
      canData1.b[2]=canBuf[2];
      canData1.b[3]=canBuf[3];
      canData2.b[0]=canBuf[4];
      canData2.b[1]=canBuf[5];
      canData2.b[2]=canBuf[6];
      canData2.b[3]=canBuf[7];

      switch (packetId) {
      case CAN_ID_SPEED:
	reportValues[1] = canData1.f; // hardcoded index into array is kind of stupid
	reportValues[0] = reportValues[1] * EHZ_scale;
	break;
      case CAN_ID_BUS_VOLT_CURR:
	reportValues[2] = canData1.f;
	reportValues[3] = canData2.f;
	break;
      case CAN_ID_TEMP_MOT_MOS1:
	reportValues[4] = canData2.f;
	break;
      default:
	break;
      }

      DEBUG_PRINT(" :: ");
      DEBUG_PRINTLN(reportValues[val]);
    }
  }
}

// right justifies digit on to the alpha display
//  this uses the hideous arduino String() function
void displayNum(int n) {
  String str = String(n);
  if (str.length() < 4) {
    for(int i=0;i < int(str.length()); i++) {
      // DEBUG_PRINTLN(str.charAt(i));
      alphaLED.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
}

void button1Status() {
  currentState1 = digitalRead(BTN_PIN1);

  if(lastState1 == HIGH && currentState1 == LOW) {// press
    pressedTime1 = millis();
  }
  else if(lastState1 == LOW && currentState1 == HIGH) { // release
    releasedTime1 = millis();

    if( releasedTime1 - pressedTime1 < SHORT_PRESS_TIME ) {
      userRequest++; if (userRequest > userRequestMax - 1) { userRequest = 0; }
      DEBUG_PRINTLN("short press1");
      delay(10);
      reportInc = 0; // set to zero
    }

    if( releasedTime1 - pressedTime1 > LONG_PRESS_TIME ) {
      DEBUG_PRINTLN("long press1");
      brightnessToggle = !brightnessToggle;
      alphaLED.setBrightness(brightnessToggle * 14);
      delay(10);
      reportInc = 0;
    }
  }

  // save the the last state
  lastState1 = currentState1;
}

void button2Status() {
  currentState2 = digitalRead(BTN_PIN2);

  if(lastState2 == HIGH && currentState2 == LOW) // press
    pressedTime2 = millis();
  else if(lastState2 == LOW && currentState2 == HIGH) { // release
    releasedTime2 = millis();

    if( releasedTime2 - pressedTime2 < SHORT_PRESS_TIME ) {
      userRequest++; if (userRequest > userRequestMax - 1) { userRequest = 0; }
      DEBUG_PRINTLN("short press2");
      delay(10);
      reportInc = 0; // set to zero
    }

    if( releasedTime2 - pressedTime2 > LONG_PRESS_TIME ) {
      DEBUG_PRINTLN("long press2");
      brightnessToggle = !brightnessToggle;
      alphaLED.setBrightness(brightnessToggle * 14);
      delay(10);
      reportInc = 0;
    }
  }

  // save the the last state
  lastState2 = currentState2;
}

// the delays make this blocking
void zipDisplay(int c) { 
  alphaLED.writeDigitRaw(3, 0x0);
  alphaLED.writeDigitRaw(0, c);
  alphaLED.writeDisplay();
  delay(100);
  alphaLED.writeDigitRaw(0, 0x0);
  alphaLED.writeDigitRaw(1, c);
  alphaLED.writeDisplay();
  delay(100);
  alphaLED.writeDigitRaw(1, 0x0);
  alphaLED.writeDigitRaw(2, c);
  alphaLED.writeDisplay();
  delay(100);
  alphaLED.writeDigitRaw(2, 0x0);
  alphaLED.writeDigitRaw(3, c);
  alphaLED.writeDisplay();
  delay(100);
  
  alphaLED.clear();
  alphaLED.writeDisplay();
}

int ifInSet(uint16_t i) {
  for (int x = 0; x < canElements; x++)  {
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

