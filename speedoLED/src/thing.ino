#include <Wire.h>
#include <Ewma.h>
#include <VescUart.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#define IDLE               0 
#define CHANGE_BRIGHTNESS  1
#define CHANGE_MODE        2
#define TEMP_WARNING       3

// #define MINVOLTAGE 88.8
// #define MAXVOLTAGE 99.6

// testing on a lower power supply
#define MINVOLTAGE 86.4
#define MAXVOLTAGE 104.0

#define MAXTEMP 65

// for MPH
#define POLEPAIRS    16
#define CIRCUMFERENCE 78.5 // much easier to measure with tape
#define INCH_IN_MILE 63360
double MPH_scale = (CIRCUMFERENCE * 60) / (POLEPAIRS * INCH_IN_MILE);

// filter system
Ewma adcFilter0(0.1);
Ewma adcFilter1(0.1);
Ewma adcFilter2(0.1);
Ewma adcFilter3(0.1);

double dataValue[4]    = {0.0, 0.0, 0.0, 0.0};
double oldDataValue[4] = {1.0, 1.0, 1.0, 1.0};
int chargeLevel = 90;
int oldChargeLevel = 100;
int oldAmpHours = 0;
int ampHours = 0;
int reading = 0; // Value to be displayed


boolean RECEIVE_BITS = false;
VescUart UART;

boolean blinkOn = false;
uint32_t blinkDelta = 0;
uint32_t blinkInterval = 700; 
uint32_t blinkNow;

bool brightnessToggle = true;
bool buttonState = false;
bool  lastReading = false;
bool readingState = false;
int readingTime = 0; // 0 is no reading; 1 is short; 2 is long;
unsigned long onTime=0;

int state = IDLE;
int inc = 0;
int maxInc = 3;

//MPH, AMPS, BATTERY, TEMP
char prefixes[] = {'M', 'A', 'B', 'T'};

const int btnPin = 12;

Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();

void setup() {
  Serial.begin(9600);
  Serial1.begin(19200);

  UART.setSerialPort(&Serial1);
  
  pinMode(btnPin, INPUT);

  alpha4.begin(0x70);
  alpha4.setBrightness(0);

  zipDisplay(0xBFFF);  zipDisplay(0xBFFF);  zipDisplay(0xBFFF);

  alpha4.writeDigitAscii(0, prefixes[inc]);
  displayNum(pow(12,inc-0));
  alpha4.writeDisplay();
}

void loop() {

  RECEIVE_BITS = false;

  if ( UART.getVescValues() ) {
    RECEIVE_BITS = true;
    dataValue[0] = adcFilter0.filter(UART.data.rpm * MPH_scale);
    dataValue[1] = adcFilter1.filter(UART.data.avgInputCurrent);
    dataValue[2] = adcFilter2.filter(100 * (UART.data.inpVoltage - MINVOLTAGE) / (MAXVOLTAGE - MINVOLTAGE));
    dataValue[3] = adcFilter3.filter(UART.data.tempFET);

    if (dataValue[3] > MAXTEMP) {
      state = TEMP_WARNING;
    }

  }
  else {
    // Serial.println("Failed to get data!");
  }

  Serial.println(dataValue[3]);

  buttonState = digitalRead(btnPin);

  if (buttonState == LOW && lastReading == HIGH) { // quick press, change inc
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

  switch (state) {
  case IDLE:
    alpha4.clear();
    if (RECEIVE_BITS) {
      alpha4.writeDigitAscii(0, prefixes[inc]);
      displayNum( int (dataValue[inc]) );
      alpha4.writeDisplay();
    }
    else {
      zipDisplay(0x00C0);
    }
    break;
  case CHANGE_BRIGHTNESS:
    alpha4.setBrightness(brightnessToggle * 14);
    brightnessToggle = !brightnessToggle;
    state = IDLE;
    break;
  case CHANGE_MODE:
    zipDisplay(0xBFFF); 
    inc++; if (inc > maxInc) { inc = 0; }
    state = IDLE;
    break;
  case TEMP_WARNING:
    alpha4.writeDigitAscii(0, 'T');
    alpha4.writeDigitAscii(1, 'E');
    alpha4.writeDigitAscii(2, 'M');
    alpha4.writeDigitAscii(3, 'P');
    alpha4.writeDisplay();
    delay(400);
    alpha4.clear();
    displayNum( int (dataValue[3]) );
    alpha4.writeDisplay();
    delay(400);
    alpha4.clear();
    state = IDLE;
    break;
  default:
    break;
  }
}

// right justifies digits
void displayNum(int n) {
  String str = String(n); // terrible arduino function
  if (str.length() < 4) {
    for(int i=0;i < str.length(); i++) {
      alpha4.writeDigitAscii(i+(4-str.length()), str.charAt(i));
    }
  }
}

void zipDisplay(int c) { // this is a little blocking
  alpha4.writeDigitRaw(3, 0x0);
  alpha4.writeDigitRaw(0, c);
  alpha4.writeDisplay();
  delay(100);
  alpha4.writeDigitRaw(0, 0x0);
  alpha4.writeDigitRaw(1, c);
  alpha4.writeDisplay();
  delay(100);
  alpha4.writeDigitRaw(1, 0x0);
  alpha4.writeDigitRaw(2, c);
  alpha4.writeDisplay();
  delay(100);
  alpha4.writeDigitRaw(2, 0x0);
  alpha4.writeDigitRaw(3, c);
  alpha4.writeDisplay();
  delay(100);
  
  alpha4.clear();
  alpha4.writeDisplay();
}


