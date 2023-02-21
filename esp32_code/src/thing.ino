#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include <Wire.h>
#include <Ewma.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 
BluetoothSerial SerialBT;

#define IDLE                0 
#define INIT                1 
#define RECEIVED_PACKET     2
#define CHANGE_BRIGHTNESS   3
#define CHANGE_MODE         4
#define TEMP_WARNING        5
#define RECEIVED_BAD_PACKET 6
#define PING_SERIAL         7
#define SET_LOG_VARIABLES   8

#define BTN_PIN1  12
#define BTN_PIN2  13

#define BAUDRATE 115200

boolean JSON_WARNING = false; 

// filter system, not used at present
Ewma adcFilter0(0.1);
Ewma adcFilter1(0.1);
Ewma adcFilter2(0.1);
Ewma adcFilter3(0.1);

// magic nums for calculation of rpm
//   calculations do not deal with gear ratios
#define POLEPAIRS    5
#define CIRCUMFERENCE 78.5 // much easier to circumerence measure with tape
#define INCH_IN_MILE 63360
double MPH_scale = (CIRCUMFERENCE * 60) / (POLEPAIRS * INCH_IN_MILE);
#define MINVOLTAGE 86.4
#define MAXVOLTAGE 104.0 // 24s lipos
#define MAXTEMP 65

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

int state = IDLE;

// used to parse input
int inputInc = 0;
char * tags [] = {
    (char*) "ehz",
    (char*) "tmp",
    (char*) "vbus",
    (char*) "tmp"
};
#define maxTags     (sizeof(tags)/sizeof(char *))
char prefixes[maxTags] = {'M', 'A', 'B', 'T'}; // MPH, AMPS, BATTERY, TEMP

// stuff to manipulate milliseconds
uint32_t milliStart, milliCurrent, milliDelta;
boolean milliReset = true;

// serial reading stuff
unsigned long timeSinceReceive;
unsigned long currentTime;
const unsigned long serialInterval = 600;

char inChar;
char inChar2;
char oldChar;
char inStr[100] = "";
char cpStr[100] = "";
int count = 0;

uint16_t amps;
uint16_t volts;

void setup() {
  Serial.begin(BAUDRATE);
  Serial1.begin(BAUDRATE);
  SerialBT.begin("ESP32 Speedometer"); // name seen by BLE receiving device

  pinMode(BTN_PIN1, INPUT);
  pinMode(BTN_PIN2, INPUT);

  alphaLED.begin(0x70);
  alphaLED.setBrightness(0);

  delay(1000); 
}

void loop() {

  checkSerial();
  checkButton1();
  checkButton2();

  switch (state) {
  case IDLE:
    currentTime = millis();
    if (currentTime - timeSinceReceive >= serialInterval) {
      zipDisplay(0xBFFF); // this is blocking by 300ms
      // ping the device
      Serial1.write("help\r\n");
    }
    break;

  case INIT:
    zipDisplay(0xBFFF); 
    displayNum(pow(12,inputInc-0));
    alphaLED.writeDisplay();
    state = IDLE;
    break;

  case RECEIVED_PACKET:
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, prefixes[inputInc]);
    {
      Serial.print("json  :: ");
      Serial.print(inStr);
      Serial.println(":: json");


      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, inStr);

      // two things can happen
      if (error) { 
	if (strstr(inStr, "Displays this help") != NULL) {
	  Serial.print("SERIAL ON  :: ");
	  Serial.print(inStr);
	  Serial1.write("status json\r\n"); // request json
	  delay(100);
	}
	else {
	  // Serial.print("noise  :: ");
	  // Serial.print(inStr);
	  // Serial.println(" :: noise");
	}
	state = IDLE;
	break;
      }
      else { // or not, but that's fine
	switch(prefixes[inputInc]) {
	case 'A': // Serial.println("AMPS!");
	  if (doc["idq_d"].isNull() != 1 & doc["idq_q"].isNull() != 1) { 
	    float iDd = doc["idq_d"];
	    float iDq = doc["idq_q"];
	    displayNum( sqrt((iDq * iDq) + (iDd * iDd)) );
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	case 'M':
	  if (doc[tags[inputInc]].isNull() != 1) { // requested thing may not have been in json
	    float x = doc[tags[inputInc]].as<float>();
	    displayNum( int( (x * 6.0) / 7) ); 
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	case 'B': // VBat
	  if (doc[tags[inputInc]].isNull() != 1) { 
	    displayNum( int( doc[tags[inputInc]].as<float>() ) ); 
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	}
      }
    }
    alphaLED.writeDisplay();

    state = IDLE;
    break;

  case CHANGE_BRIGHTNESS:
    alphaLED.setBrightness(brightnessToggle * 14);
    brightnessToggle = !brightnessToggle;
    state = IDLE;
    break;

  case CHANGE_MODE:
    zipDisplay(0xBFFF); 
    Serial.print("new inc: ");
    Serial.println(prefixes[inputInc]);
    // SerialBT.print("new state: ");
    // SerialBT.println(prefixes[inputInc]);
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, prefixes[inputInc]);
    alphaLED.writeDisplay();

    state = IDLE;
    break;

  case TEMP_WARNING:
    alphaLED.writeDigitAscii(0, 'T');
    alphaLED.writeDigitAscii(1, 'E');
    alphaLED.writeDigitAscii(2, 'M');
    alphaLED.writeDigitAscii(3, 'P');
    alphaLED.writeDisplay();
    delay(400);
    alphaLED.clear();
    // displayNum( int (dataValue[3]) );
    alphaLED.writeDisplay();
    delay(400);
    alphaLED.clear();
    state = IDLE;
  default:
    break;
  }
}

void checkSerial() {
  // receives input from the ESC
  if (Serial1.available()) {
    inChar = Serial1.read();
    // sometimes non-useful characters come in
    if (inChar == 10 || inChar == 13 || (inChar > 31 && inChar < 127)) {
      if (inChar == '\n' && oldChar == '\r') {
	// must have received end of line if we're here
	inStr[count - 1] = '\0';
	timeSinceReceive = millis();
	state = RECEIVED_PACKET;
	count = 0;
      }
      else {
	inStr[count] = inChar;
	count++;
	oldChar = inChar;
      }
    }
  }

  // manually talk to serial if needed
  if (Serial.available()) {
    inChar2 = Serial.read();
    Serial.write(inChar2);
    Serial1.write(inChar2);
  }
}

void checkButton1() {
  buttonState1 = digitalRead(BTN_PIN1);

  if (buttonState1 == LOW && lastReading1 == HIGH) { // quick press, change inputInc
    onTime1 = millis();
    readingState1 = true;
    readingTime1 = 1;
  }

  if (buttonState1 == LOW && lastReading1 == LOW) { // long press, toggle brightness
    if ((millis() - onTime1) > 500 ) {
      lastReading1 = LOW;
      readingTime1 = 2;
      readingState1 = true;
    } 
  }

  if (readingState1 == true && buttonState1 == HIGH) {
    if (readingTime1 == 1) {
      Serial.println("short1 high");
      inputInc++; if (inputInc > maxTags - 1) { inputInc = 0; }
      state = CHANGE_MODE;
    }
    if (readingTime1 == 2) {
      Serial.println("long2");
      state = CHANGE_BRIGHTNESS;
    }
    readingTime1 = 0;
    readingState1 = false;
  }
  lastReading1 = buttonState1;
}

void checkButton2() {
  buttonState2 = digitalRead(BTN_PIN2);

  if (buttonState2 == LOW && lastReading2 == HIGH) { // quick press, change inputInc
    onTime2 = millis();
    readingState2 = true;
    readingTime2 = 1;
  }

  if (buttonState2 == LOW && lastReading2 == LOW) { // long press, toggle brightness
    if ((millis() - onTime2) > 500 ) {
      lastReading2 = LOW;
      readingTime2 = 2;
      readingState2 = true;
    } 
  }

  if (readingState2 == true && buttonState2 == HIGH) {
    if (readingTime2 == 1) {
      Serial.println("short1 low");
      inputInc--; if (inputInc < 0) { inputInc = maxTags - 1; }
      state = CHANGE_MODE;
    }
    if (readingTime2 == 2) {
      Serial.println("long2");
      // nothing happens
    }
    readingTime2 = 0;
    readingState2 = false;
  }
  lastReading2 = buttonState2;
}

// right justifies digit on to the alpha display
//  this uses the hideous arduino String() function
void displayNum(int n) {
  String str = String(n);
  if (str.length() < 4) {
    for(int i=0;i < str.length(); i++) {
      alphaLED.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
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


