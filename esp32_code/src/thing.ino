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

#define BTN_PIN  12

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
bool buttonState = false;
bool lastReading = false;
bool readingState = false;
int readingTime = 0; // 0 is no reading; 1 is short; 2 is long;
unsigned long onTime = 0;

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

// serial reading stuff
boolean RECEIVE_BITS = false;
char inChar;
char oldChar;
char inStr[100] = "";
int count = 0;

uint16_t amps;
uint16_t volts;

void setup() {
  Serial.begin(BAUDRATE);
  Serial1.begin(BAUDRATE);
  SerialBT.begin("ESP32 Speedometer"); // name seen by BLE receiving device

  pinMode(13, OUTPUT);
  pinMode(BTN_PIN, INPUT);

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
  checkButtons();

  switch (state) {
  case IDLE:
    if (RECEIVE_BITS) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, inStr);

      alphaLED.clear();
      if (error) { // received bad json
	Serial.print(F("deserializeJson() failed: "));
	Serial.println(error.f_str());
	SerialBT.println("deserializeJson() failed: ");
	alphaLED.writeDigitAscii(3, ':');
	alphaLED.writeDigitAscii(2, '-');
	alphaLED.writeDigitAscii(1, '(');
      }
      else { 
	alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	if (doc[tags[inputInc]].isNull() != 1) { 
	  displayNum( doc[tags[inputInc]].as<int>()); 
	}
	else { // the json didnt have one of the words in tags[]
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
      SerialBT.println(inStr);
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

void checkButtons() {
  buttonState = digitalRead(BTN_PIN);

  if (buttonState == LOW && lastReading == HIGH) { // quick press, change inputInc
    onTime = millis();
    readingState = true;
    readingTime = 1;
  }

  if (buttonState == LOW && lastReading == LOW) { // long press, toggle brightness
    if ((millis() - onTime) > 500 ) {
      lastReading = LOW;
      readingTime = 2;
      readingState = true;
    } 
  }

  if (readingState == true && buttonState == HIGH) {
    if (readingTime == 1) {
      Serial.println("short");
      state = CHANGE_MODE;
    }
    if (readingTime == 2) {
      Serial.println("long");
      state = CHANGE_BRIGHTNESS;
    }
    readingTime = 0;
    readingState = false;
  }
  lastReading = buttonState;
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


