#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
// from: MESC_Firmware/MESC_RTOS/Tasks/can_ids.h
#include <can_ids.h> 
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 

#define IDLE                0 
#define REQUEST_START       1 
#define RECEIVED_PACKET     2
#define CHANGE_BRIGHTNESS   3
#define CHANGE_MODE         4
#define TEMP_WARNING        5
#define ERROR_WARNING       6
#define RECEIVED_BAD_PACKET 7
#define KILLSTATE           8

#define BTN_PIN1       12
#define BTN_PIN2       13
#define KILLSWITCH_PIN 17

#define CAN_PACKET_SIZE  8
#define CAN_DEBUG 0
#define CAN_ELEMENTS 3

// magic nums for calculation of rpm
//   calculations do not deal with gear ratios
#define POLEPAIRS    7
#define CIRCUMFERENCE 78.5 // much easier to circumerence measure with tape
#define INCH_IN_MILE 63360
double MPH_scale = (CIRCUMFERENCE * 60) / (POLEPAIRS * INCH_IN_MILE);
#define MINVOLTAGE 86.4
#define MAXVOLTAGE 104.0 // 24s lipos
#define MAXTEMP 70

unsigned long currentTime;

// CAN variables
unsigned long canLastTime;
unsigned long canInterval = 2000;
boolean receivedPacket = true;
const unsigned long canlInterval = 600;
uint16_t canSet[] = {CAN_ID_SPEED, CAN_ID_MOTOR_CURRENT, CAN_ID_TEMP_MOT_MOS1};
float canValues[CAN_ELEMENTS] = {};
uint8_t canBuf[8] = {};
uint16_t packetId;

union {
  uint8_t b[4];
  float f;
} canData;

// reporting things
uint8_t prefixes[] = {'M', 'A', 'T'}; // maintain the order with canSet[]. 'M' is SPEED, etc
uint8_t userRequest = 0; // what the user wants
uint8_t reportInc = 0; // cycles through things to display
uint8_t reportMax = 4; 
byte reportList  = B00000001; 
byte reportCAN   = B00000010; // set when exceeded timeout
byte reportError = B00000100; // set when CAN says there's an error
byte reportTemp  = B00001000; // set when over heating

// blink variables
boolean blinkOn = false;
uint32_t lastBlinkTime = 0;
uint32_t blinkInterval = 300; 
uint32_t blinkNow;

// controls if the LED is bright or dim
bool brightnessToggle = false;

// button reading timers
bool buttonState1 = false;                                                                                                  
bool lastReading1 = false;
bool readingState1 = false;
int readingTime1 = 0; 
unsigned long onTime1 = 0;

bool buttonState2 = false;
bool lastReading2 = false;
bool readingState2 = false;
int readingTime2 = 0; 
unsigned long onTime2 = 0;

int state = REQUEST_START;

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

  CAN.onReceive(onReceive); // register CAN callback

  alphaLED.begin(0x70);
  alphaLED.setBrightness(brightnessToggle * 14);
  alphaLED.clear();
  alphaLED.writeDigitAscii(1, 'O');
  alphaLED.writeDigitAscii(2, 'F');
  alphaLED.writeDigitAscii(3, 'F');
  alphaLED.writeDisplay();

  delay(1000); 
}

void loop() {

  blinkLED();
  canStatus();
  tempStatus();
  // button1Status();
  // button2Status();

  currentTime = millis();
  if ((currentTime - reportLastTime) > reportInterval) {  // went over time
    reportLastTime = currentTime;
    bumpReportInc();
  }

  switch (reportInc) {
  case 0: // show what the user wants
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, prefixes[userRequest]);
    displayNum( int( canValues[userRequest] ) ); 
    break;
  case 1: // show CAN status
    if (0) {
    }
    else { // nothing to report so...
      bumpReportInc();
    }
    break;
  case 2: // show error status
    if (0) {
    }
    else { // nothing to report so...
      bumpReportInc();
    }
    break;
  case 3: // show temp status
    if (0) {
    }
    else { // nothing to report so...
      bumpReportInc();
    }
    break;
  default:
    break;
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
  }

  currentTime = millis();
  reportList &= (~reportCAN); // clear
  if ((currentTime - canReceiveTime ) > canInterval) {  // went over time
    canCurrentTime = canReceiveTime;
    reportList |= reportCAN; // set 
  }
}

void tempStatus() {
}


void blinkLED() {
  // non-blocking blink
  currentTime= millis();
  if ((currentTime - lastBlinkTime) > blinkInterval) {
    lastBlinkTime = blinkNow;
    blinkOn = !blinkOn;
    digitalWrite(LED_BUILTIN, blinkOn);
  }
}

void onReceive(int packetSize) {
  receivedPacket = true;
  // received a packet
  packetId = extract_id(CAN.packetId());
  if (packetSize == CAN_PACKET_SIZE) {
    int val = ifInSet(packetId); // shitty CAN filter
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
      // Serial.print(str.charAt(i));
      alphaLED.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
  // Serial.println();
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

