#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4(); 
BluetoothSerial SerialBT;

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

#define BAUDRATE 115200

// magic nums for calculation of rpm
//   calculations do not deal with gear ratios
#define POLEPAIRS    7
#define CIRCUMFERENCE 78.5 // much easier to circumerence measure with tape
#define INCH_IN_MILE 63360
double MPH_scale = (CIRCUMFERENCE * 60) / (POLEPAIRS * INCH_IN_MILE);
#define MINVOLTAGE 86.4
#define MAXVOLTAGE 104.0 // 24s lipos
#define MAXTEMP 70


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

// used to parse input
int inputInc = 0;
char * tags [] = {
    (char*) "ehz",
    (char*) "tmp",
    (char*) "vbus",
    (char*) "MOS_temp"
};

#define maxTags     (sizeof(tags)/sizeof(char *))
char prefixes[maxTags] = {'M', 'A', 'B', 'T'}; // MPH, AMPS, BATTERY, TEMP

// serial reading stuff
#define STR_LEN 1024

char inChar;
char inChar2;
char oldChar;
char inStr[STR_LEN] = "";
char cpStr[STR_LEN] = "";
int count = 0;

unsigned long timeSinceReceive;
unsigned long currentTime;
const unsigned long serialInterval = 600;

int state = REQUEST_START;

void setup() {
  Serial.begin(BAUDRATE);
  // name seen by BLE receiving device
  SerialBT.begin("no receive"); 

  pinMode(BTN_PIN1, INPUT);
  pinMode(BTN_PIN2, INPUT);
  pinMode(KILLSWITCH_PIN, OUTPUT);

  digitalWrite(KILLSWITCH_PIN, HIGH);

  alphaLED.begin(0x70);
  alphaLED.setBrightness(0);
}

void loop() {

  checkSerial();
  checkButton1();
  checkButton2();

  switch (state) {
  case IDLE:
    currentTime = millis();
    if (currentTime - timeSinceReceive >= serialInterval) {
      // alphaLED.clear();
      // alphaLED.writeDisplay();
      timeSinceReceive = millis();
    }

    state = IDLE;
    break;

  case REQUEST_START:
    digitalWrite(KILLSWITCH_PIN, HIGH);
    Serial.write("start\n");
    alphaLED.clear();
    alphaLED.writeDigitAscii(1, 'R');
    alphaLED.writeDigitAscii(2, 'U');
    alphaLED.writeDigitAscii(3, 'N');
    alphaLED.writeDisplay();
    state = IDLE;
    break;
    
  case RECEIVED_PACKET:
    {
      strcpy(cpStr, inStr);
      StaticJsonDocument<200> doc;
      DeserializationError complaint = deserializeJson(doc, inStr);

      // two things can happen
      if (complaint) { 
	Serial.println("not json");
	state = IDLE;
	break;
      }
      else { // or not, but that's fine
	timeSinceReceive = millis();
	Serial.println("received json");

	float temp = doc[tags[inputInc]].as<float>();
	temp = temp - 273.15;
	if (temp > MAXTEMP) {
	  Serial.print("temp "); Serial.println(temp);
	  state = TEMP_WARNING;
	  break;
	}

	int e = doc["error"].as<int>();
	if (e != 0) {
	  Serial.print("error "); Serial.println(e);
	  alphaLED.clear();
	  alphaLED.writeDigitAscii(0, 'E');
	  displayNum( e );
	  alphaLED.writeDisplay();
	  state = IDLE;
	  break;
	}

	switch(prefixes[inputInc]) {
	case 'A': 
	  alphaLED.clear();
	  alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	  if (doc["idq_d"].isNull() != 1 & doc["idq_q"].isNull() != 1) { 
	    float iDd = doc["idq_d"];
	    float iDq = doc["idq_q"];
	    displayNum( sqrt((iDq * iDq) + (iDd * iDd)) );
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	case 'T':
	  {
	    float temp = doc[tags[inputInc]].as<float>();
	    temp = temp - 273.15;
	    alphaLED.clear();
	    alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	    if (doc[tags[inputInc]].isNull() != 1) { 
	      displayNum( int( temp ) ); 
	    }
	    else {
	      alphaLED.writeDigitAscii(3, '-');
	    }
	  }
	  break;
	case 'M':
	  alphaLED.clear();
	  alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	  if (doc[tags[inputInc]].isNull() != 1) { 
	    float x = doc[tags[inputInc]].as<float>();
	    displayNum( int( x ) ); 
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	case 'B': // VBat
	  alphaLED.clear();
	  alphaLED.writeDigitAscii(0, prefixes[inputInc]);
	  if (doc[tags[inputInc]].isNull() != 1) { 
	    displayNum( int( doc[tags[inputInc]].as<float>() ) ); 
	  }
	  else {
	    alphaLED.writeDigitAscii(3, '-');
	  }
	  break;
	}
	alphaLED.writeDisplay();
      }
    }

    state = IDLE;
    break;

  case CHANGE_BRIGHTNESS:
    brightnessToggle = !brightnessToggle;
    alphaLED.setBrightness(brightnessToggle * 14);
    state = IDLE;
    break;

  case CHANGE_MODE:
    state = REQUEST_START;
    break;

  case KILLSTATE:
    digitalWrite(KILLSWITCH_PIN, LOW);
    alphaLED.clear();
    alphaLED.writeDigitAscii(1, 'O');
    alphaLED.writeDigitAscii(2, 'F');
    alphaLED.writeDigitAscii(3, 'F');
    alphaLED.writeDisplay();
    state = KILLSTATE;
    break;

  case TEMP_WARNING:
    alphaLED.clear();
    alphaLED.writeDigitAscii(0, 'T');
    alphaLED.writeDigitAscii(1, 'E');
    alphaLED.writeDigitAscii(2, 'M');
    alphaLED.writeDigitAscii(3, 'P');
    alphaLED.writeDisplay();
    state = IDLE;
    break;

  default:
    break;
  }
}

void checkSerial() {
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
      // inputInc++; if (inputInc > maxTags - 1) { inputInc = 0; }
      state = KILLSTATE;
    }
    if (readingTime1 == 2) {
      Serial.println("long1");
      state = REQUEST_START;
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
      Serial.println("short2 low");
      inputInc--; if (inputInc < 0) { inputInc = maxTags - 1; }
      state = CHANGE_MODE;
    }
    if (readingTime2 == 2) {
      Serial.println("long2");
      state = CHANGE_BRIGHTNESS;
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


