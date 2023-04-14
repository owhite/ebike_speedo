#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <can_ids.h> // from: MESC_Firmware/MESC_RTOS/Tasks/can_ids.h
#include <Adafruit_LEDBackpack.h>

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 

#define BTN_PIN1       12
#define BTN_PIN2       13
#define KILLSWITCH_PIN 17

#define CAN_PACKET_SIZE  8
#define CAN_DEBUG 0
#define CAN_ELEMENTS 3

// bike variables
#define POLEPAIRS    7
#define CIRCUMFERENCE 78.5 // much easier to circumerence measure with tape
#define INCH_IN_MILE 63360
#define MINVOLTAGE 86.4
#define MAXVOLTAGE 104.0 // 24s lipos
#define MAXTEMP 70
double MPH_scale = (CIRCUMFERENCE * 60) / (POLEPAIRS * INCH_IN_MILE);

unsigned long currentTime;

// CAN variables
unsigned long canReceiveTime;
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
unsigned long reportLastTime;
unsigned long reportInterval = 400;
uint8_t prefixes[] = {'M', 'A', 'T'}; // maintain the order with canSet[]. 'M' is SPEED, etc

uint8_t userRequest = 0; // what the user wants
uint8_t userRequestMax = 3; 

uint8_t reportInc = 0; // cycles through things to display
uint8_t reportMax = 4; 
boolean reportCAN = false;    // set when exceeded timeout
boolean reportError = true; // set when CAN says there's an error
boolean reportTemp  = true;  // set when over heating

// blink variables
boolean blinkOn = false;
uint32_t lastBlinkTime = 0;
uint32_t blinkInterval = 100; 
uint32_t blinkNow;

// control if LED is bright or dim
bool brightnessToggle = false;

// button reading timers
const int SHORT_PRESS_TIME = 600;
const int LONG_PRESS_TIME  = 600;
int lastState1 = LOW;
int currentState1;
unsigned long pressedTime1  = 0;
unsigned long releasedTime1 = 0;

int lastState2 = LOW;
int currentState2;
unsigned long pressedTime2  = 0;
unsigned long releasedTime2 = 0;


void setup() {
  Serial.begin(9600);

  pinMode(BTN_PIN1, INPUT);
  pinMode(BTN_PIN2, INPUT);
  pinMode(PIN_CAN_STANDBY, OUTPUT);
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false); // standby off
  digitalWrite(PIN_CAN_BOOSTEN, true);  // booster on
  
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

  zipDisplay(0xBFFF); 
  zipDisplay(0xBFFF); 
  zipDisplay(0xBFFF); 
}

void loop() {

  blinkLED();
  canStatus();
  tempStatus();
  errorStatus();
  button1Status();
  // button2Status();

  switch (reportInc) {
  case 0: // show what the user wants
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, prefixes[userRequest]);
    displayNum( int( canValues[userRequest] ) ); 
    alphaLED.writeDisplay();
    break;
  case 1: // show CAN status
    if (reportCAN) {
      alphaLED.clear();
      alphaLED.writeDigitAscii(1, 'C');
      alphaLED.writeDigitAscii(2, 'A');
      alphaLED.writeDigitAscii(3, 'N');
      alphaLED.writeDisplay();
      // Serial.println("no CAN");
    }
    break;
  case 2: // show error status
    if (reportError) {
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, 'E');
      displayNum( int( canValues[userRequest] ) ); 
      // displayNum( int( canValues[userRequest] ) ); 
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

void tempStatus() {
  reportTemp = false;
}

void errorStatus() {
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
    for(int i=0;i < int(str.length()); i++) {
      // Serial.print(str.charAt(i));
      alphaLED.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
  // Serial.println();
}

void button1Status() {
  currentState1 = digitalRead(BTN_PIN1);

  if(lastState1 == HIGH && currentState1 == LOW) // press
    pressedTime1 = millis();
  else if(lastState1 == LOW && currentState1 == HIGH) { // release
    releasedTime1 = millis();

    if( releasedTime1 - pressedTime1 < SHORT_PRESS_TIME ) {
      userRequest++; if (userRequest > userRequestMax - 1) { userRequest = 0; }
      Serial.println("short press");
      delay(10);
      reportInc = 0; // set to zero
    }

    if( releasedTime1 - pressedTime1 > LONG_PRESS_TIME ) {
      Serial.println("long press");
      brightnessToggle = !brightnessToggle;
      alphaLED.setBrightness(brightnessToggle * 14);
      delay(10);
      reportInc = 0;
    }
  }

  // save the the last state
  lastState1 = currentState1;
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

