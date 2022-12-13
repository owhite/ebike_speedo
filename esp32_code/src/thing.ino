#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include <Wire.h>
#include <Ewma.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 
BluetoothSerial SerialBT;

#define IDLE               0 
#define CHANGE_BRIGHTNESS  1
#define CHANGE_MODE        2
#define TEMP_WARNING       3

#define BTN_PIN1  12
#define BTN_PIN2  13

#define BAUDRATE 115200

// filter system
Ewma adcFilter0(0.1);
Ewma adcFilter1(0.1);
Ewma adcFilter2(0.1);
Ewma adcFilter3(0.1);

// magic nums for calculation of rpm
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
    (char*) "amps",
    (char*) "volts",
    (char*) "rpm",
    (char*) "temp"
};
#define maxTags     (sizeof(tags)/sizeof(char *))
char prefixes[maxTags] = {'M', 'A', 'B', 'T'}; // MPH, AMPS, BATTERY, TEMP

// stuff to manipulate milliseconds
uint32_t milliStart, milliCurrent, milliDelta;
boolean milliReset = true;

// serial reading stuff
boolean RECEIVE_BITS = false;
char inChar;
char oldChar;
char inStr[100] = "";
char cpStr[100] = "";
int count = 0;
int loopCount = 0;

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

  zipDisplay(0xBFFF);  zipDisplay(0xBFFF);  zipDisplay(0xBFFF);

  alphaLED.writeDigitAscii(0, prefixes[inputInc]);
  displayNum(pow(12,inputInc-0));
  alphaLED.writeDisplay();
}

void loop() {

  RECEIVE_BITS = false;
  checkSerial();
  checkButton1();
  checkButton2();

  switch (state) {
  case IDLE:
    if (RECEIVE_BITS) {

      strcpy(cpStr, inStr); // need a backup
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, inStr);

      alphaLED.clear();
      if (error) { // received bad json
	Serial.print(F("deserializeJson() failed: "));
	Serial.println(error.f_str());
	SerialBT.println("deserializeJson() failed: ");
	SerialBT.println(error.f_str());
	SerialBT.println(cpStr);
	// alert speedo user there's a _JSON error_
	alphaLED.writeDigitAscii(0, 'J'); 
	alphaLED.writeDigitAscii(1, 'S');
	alphaLED.writeDigitAscii(2, 'N');
	alphaLED.writeDigitAscii(3, 'E');
      }
      else { // no JSON error
	if (doc["mS"].isNull() != 1) { // did we get milliseconds? 
	  milliCurrent = doc["mS"].as<uint32_t>();
	  // user requested that we zero out the milliseconds
	  if (milliReset) { milliStart = milliCurrent; }
	  // stuff it back into the json
	  doc["mS"] = (milliCurrent - milliStart); 
	}
	milliReset = false;

	// serialize and send to the BLE
	serializeJson(doc, SerialBT);            
	SerialBT.println();

	alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	if (doc[tags[inputInc]].isNull() != 1) { 
	  displayNum( doc[tags[inputInc]].as<int>()); 
	}
	else { // one of the words in tags[] was not in json
	  alphaLED.writeDigitAscii(3, '-');
	}
      }
      alphaLED.writeDisplay();
    }
    break;
  case CHANGE_BRIGHTNESS:
    alphaLED.setBrightness(brightnessToggle * 14);
    brightnessToggle = !brightnessToggle;
    state = IDLE;
    break;
  case CHANGE_MODE:
    zipDisplay(0xBFFF); 
    inputInc++; if (inputInc > maxTags - 1) { inputInc = 0; }
    // SerialBT.print("new state: ");
    // SerialBT.println(prefixes[inputInc]);
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
  if (Serial1.available()) {
    inChar = Serial1.read();
    if (inChar == '\n' && oldChar == '\r') {
      inStr[count - 1] = '\0';
      // Serial.println(inStr);
      Serial.print("thing  ");
      Serial.println(inStr);

      RECEIVE_BITS = true;
      count = 0;
    }
    else {
      inStr[count] = inChar;
      count++;
    }
    // Serial.write(inChar);
    if (inChar != '\n') {
      // SerialBT.write(inChar);
    }
    oldChar = inChar;
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
      Serial.println("short1");
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
      // user is requesting to reset reported time
      milliReset = true;
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

// right justifies digits
//  uses the hideous arduino String() function
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


