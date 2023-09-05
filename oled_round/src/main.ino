// Libraries required to make this thing run.
//  respect all licensing associated with these libraries:
#include <FlexCAN_T4.h>
#include <MenuSystem.h> 
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>
#include <EEPROM.h>
// Other custom code blocks
#include <display_handling.h> // my collection of stupid things
#include <menu_handling.h>
#include <bitmaps.h>
#include <MESCerror.h>
#include <states.h>
#include <can_ids.h> // from Jens' code

// BIKE setup, no I'm not storing all this in eeprom
#define BIKE_SPEED_MAX 50
#define POLEPAIRS    7
#define CIRCUMFERENCE 219.5 
#define GEAR_RATIO 9.82
#define CM_IN_MILE 160900
#define MINVOLTAGE 40.0
#define MAXVOLTAGE 60.0
#define MAXTEMP 70
#define MAXSPEED 50
float EHZ_SCALE_M = (CIRCUMFERENCE * 60 * 60) / (CM_IN_MILE * GEAR_RATIO * POLEPAIRS);
float EHZ_SCALE_K = (CIRCUMFERENCE * 60 * 60) / (100000 * GEAR_RATIO * POLEPAIRS);

// eeprom storage positions
#define RESET_STORED_VALUES 0x0000 // change to 1 if store new values
#define PIX_BRIGHTNESS_ADDR 0x0011
#define PIX_COLOR_ADDR      0x0012
#define DISPLAY_STATE_ADDR  0x0013
#define METRIC_PREF_ADDR    0x0014

// joystick button setup
const uint8_t btnMax = 5;
elapsedMillis elapsedBtnTime[btnMax] = {elapsedMillis(), elapsedMillis(), elapsedMillis(), elapsedMillis(), elapsedMillis()};
uint16_t btnDelay = 50;
boolean btnPressed[] = {false, false, false, false, false};
uint8_t btnPin[5] = {19, 21, 18, 17, 16};
uint8_t btnNum = 0;

// neopixel setup
#define PIXELPIN   2
#define NUMPIXELS 24
Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

const uint8_t PIX_RED   = 0;
const uint8_t PIX_GREEN = 1;
const uint8_t PIX_BLUE  = 2;
const uint8_t PIX_WHITE = 3; 

uint32_t colorArray[] = {ring.Color(255, 0, 0), ring.Color(0, 255, 0), ring.Color(0, 0, 255), ring.Color(255, 255, 255)};

// gets changed later by eepromDefaultValues();
uint8_t metric_pref = 0;
uint8_t pixelColor = PIX_WHITE;
uint8_t neoBrightVal = 5;

// round oled setup
//  see Adafruit_GC9A01A github for pin usage
#define LCD_BL_PIN 5 // pin used for brightness
Adafruit_GC9A01A lcd(10, 9, 11, 13, 8, 12);

// CAN stuff
#define CAN_PACKET_SIZE  8

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can;
CAN_message_t msg;
unsigned long canReceiveTime;
const unsigned long canInterval = 2000;
uint8_t canBuf[8] = {};
const uint8_t userRequestMax = 5; 
float reportedValue; 
float reportValues[userRequestMax] = {};
bool canFlag = false;
bool errorFlag = false;
bool canFlag_old = false;
bool errorFlag_old = false;

// Digifont setup, set up here but used in displayHandlder object. 
void customLineH(int x0,int x1, int y, int c)    { lcd.drawFastHLine(x0,y,x1-x0+1,c); }
void customLineV(int x, int y0,int y1, int c)    { lcd.drawFastVLine(x,y0,y1-y0+1,c); } 
void customRect(int x, int y,int w,int h, int c) { lcd.fillRect(x,y,w,h,c); } 
DigiFont font(customLineH, customLineV, customRect);

// display things
int input_state = INPUT_MENU;
int state = IDLE_DISPLAY;

displayHandler display;

uint32_t dataInterval = 4000; 
int main_display_state = MAIN_DISPLAY_BAT;
uint8_t brightness_val = 10;
int count = 0;
bool entered_menu = true;
float level_old = 0.0;
float level = 0.8;

// menu setup
MyRenderer my_renderer;
MenuSystem ms(my_renderer);

Menu       mu1    ("Display");
Menu       mu2    ("Errors");
MenuItem   mu3    ("Brightness",   [] { changeState(INIT_BRIGHTNESS); });
Menu       mu4    ("Ring Colors");

MenuItem   mu1_mi1("Amps" , [] { updateMainDisplay(MAIN_DISPLAY_AMPS);  });
MenuItem   mu1_mi2("BAT  ", [] { updateMainDisplay(MAIN_DISPLAY_BAT); });
MenuItem   mu1_mi3("ehz"  , [] { updateMainDisplay(MAIN_DISPLAY_EHZ);   });
MenuItem   mu1_mi4("TEMP" , [] { updateMainDisplay(MAIN_DISPLAY_TEMP);  });
MenuItem   mu1_mi5("MPH"  , [] { updateMainDisplay(MAIN_DISPLAY_MPH);   });
MenuItem   mu1_mi6("KPH"  , [] { updateMainDisplay(MAIN_DISPLAY_KPH);   });
MenuItem   mu1_mi7("FW"   , [] { updateMainDisplay(MAIN_DISPLAY_FW);    });

MenuItem   mu2_mi1("View errors",  [] { changeState(INIT_ERRORS); });
MenuItem   mu2_mi2("Reset errors", &errorReset);

MenuItem   mu4_mi1("RED" ,  [] { changeLEDColor(PIX_RED, "red"); });
MenuItem   mu4_mi2("GREEN", [] { changeLEDColor(PIX_GREEN, "green"); });
MenuItem   mu4_mi3("BLUE",  [] { changeLEDColor(PIX_BLUE, "blue"); });
MenuItem   mu4_mi4("WHITE", [] { changeLEDColor(PIX_WHITE, "white"); });

// menu callbacks
void changeLEDColor(uint32_t color, char *str) {
  Serial.print("LED color :: ");
  Serial.println( color );

  lcd.fillScreen(BLACK);
  lcd.setTextSize(2);
  lcd.setTextColor(WHITE);
  lcd.setCursor(20, 6 * LCD_CHAR_HEIGHT);
  lcd.print("change color:");
  lcd.setCursor(20, 8 * LCD_CHAR_HEIGHT);
  lcd.print(str);
  pixelColor = color;
  updateRingBrightness();

  delay(1000); // pause to look at LCD

  EEPROM.write(PIX_COLOR_ADDR, color);

  display.setupFrame(main_display_state);
  // this used to be false, changing this to see what happens
  // entered_menu = false; 
  entered_menu = true; 
  state = INIT_DISPLAY;
}

void updateMainDisplay(int val) {
  Serial.print("diplay change :: ");
  Serial.println( val );

  lcd.setTextSize(2);
  lcd.setCursor(20, 6 * LCD_CHAR_HEIGHT);
  lcd.setTextColor(WHITE);
  lcd.fillScreen(BLACK);
  lcd.print("set main disp:");
  display.drawRGBBitmap(32, 8 * LCD_CHAR_HEIGHT, bitmap[val], BITMAP_WIDTH, BITMAP_HEIGHT);
  delay(1000); // pause to look at LCD

  main_display_state = val;
  EEPROM.write(DISPLAY_STATE_ADDR, main_display_state);

  display.setupFrame(main_display_state);
  // this used to be false, changing this to see what happens
  // entered_menu = false; 
  entered_menu = true;
  state = INIT_DISPLAY;
}

void changeState(int val) {
  Serial.print("state change :: ");
  Serial.println( val );

  state = val;

  entered_menu = true;

  lcd.setTextSize(1);
  lcd.setCursor(0, 6 * LCD_CHAR_HEIGHT);
  lcd.setTextColor(WHITE);
  // lcd.print("state change: ");
  // lcd.print(state);

  delay(500); // pause to look at LCD
}

// doesnt work yet -- hope to tell MESC to reset the error someday
void errorReset(MenuComponent* p_menu_component) {
  lcd.setTextColor(WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(20, 8 * LCD_CHAR_HEIGHT);
  lcd.print("Error code reset");
  lcd.setCursor(24, 10 * LCD_CHAR_HEIGHT);
  lcd.print("...would be cool");
  Serial.println("Error reset selected");
  state = IDLE_MENU;
}

void setup() {
  Serial.begin(9600);
  can.begin();
  can.setBaudRate(500000);

  eepromDefaultValues();

  pinMode(LCD_BL_PIN, OUTPUT); // LCD brightness
  digitalWrite(LCD_BL_PIN, HIGH);

  lcd.begin();
  lcd.setRotation(0);
  lcd.fillScreen(BLACK);
  lcd.setTextSize(1);

  ring.begin(); 
  ring.setBrightness(neoBrightVal);
  ring.clear();
  ring.show(); // one LED flashes for some reason
  ring.clear();
  ring.show(); 

  for (int i = 0; i < 5; i++) { pinMode(btnPin[i], INPUT); }

  // some parts parts of code change the LCD in the displayHandler
  //  while other parts do it here in main. not clever, but ¯\_(ツ)_/¯
  display.begin(&lcd, &font, &ring);

  display.showStarField();

  state = INIT_DISPLAY;

  my_renderer.initLCD(&lcd);
  ms.get_root_menu().add_menu(&mu1, RENDER_LIST);
  mu1.add_item(&mu1_mi1);
  mu1.add_item(&mu1_mi2);
  mu1.add_item(&mu1_mi3);
  mu1.add_item(&mu1_mi4);
  mu1.add_item(&mu1_mi5);
  mu1.add_item(&mu1_mi6);
  mu1.add_item(&mu1_mi7);

  ms.get_root_menu().add_menu(&mu2, RENDER_LIST);
  mu2.add_item(&mu2_mi1);
  mu2.add_item(&mu2_mi2);

  ms.get_root_menu().add_item(&mu3);

  ms.get_root_menu().add_menu(&mu4, RENDER_LIST);
  mu4.add_item(&mu4_mi1);
  mu4.add_item(&mu4_mi2);
  mu4.add_item(&mu4_mi3);
  mu4.add_item(&mu4_mi4);

  entered_menu = true;
}

void loop() {
  // menu timeout
  state = ((millis() - ms.get_last_menu_time() ) > dataInterval) ? INIT_DISPLAY : state;

  // can timeout
  canFlag = ((millis() - canReceiveTime ) > canInterval) ? true : false;

  level = reportValues[2];

  serial_input();
  can_input();
  joystick();

  switch (state) {
  case INIT_MENU:
    {
      entered_menu = true;
      ms.display();
      state = IDLE_MENU;
      break;
    }
  case INIT_DISPLAY: // decides to repaint display or go to idle_display
    {
      if (entered_menu) {
	display.setupFrame(main_display_state);
	display.updateBattery(reportValues[2], MINVOLTAGE, MAXVOLTAGE);
	entered_menu = false;
      }
      display.updateCANErrorFlags(canFlag, errorFlag);
      // if the user timesouts the menu, menu is reset to top
      input_state = INPUT_MENU;
      ms.reset(); 
      state = IDLE_DISPLAY;
    }
    break;
  case INIT_BRIGHTNESS:
    {
      input_state = INPUT_NUMBER;
      lcd.fillScreen(BLACK);
      lcd.setTextSize(2);
      lcd.setTextColor(WHITE);
      lcd.setCursor(20, 6 * LCD_CHAR_HEIGHT);
      lcd.print("brightness");
      lcd.setCursor(20, 8 * LCD_CHAR_HEIGHT);
      lcd.print("use up/down/back");
      Serial.println("BRIGHT!");
      ring.clear();

      // joystick & serial handlers take it from here
      state = IDLE_MENU;
    }
    break;
  case INIT_ERRORS:
    {
      lcd.fillScreen(BLACK);
      lcd.setTextSize(2);
      int y = 40;

      lcd.setCursor(30, y);
      lcd.setTextColor(WHITE);
      lcd.print("MESC ERRORS:");

      y += 2 * LCD_CHAR_HEIGHT;

      int error_val = 10;

      for(int i=0;i<32;i++) {
	if (error_val & 0x0001) {
	  lcd.setTextSize(2);
	  lcd.setCursor(20, y);
	  y += (2 * LCD_CHAR_HEIGHT);
	  lcd.print(error_codes[i]);
	  Serial.println(error_codes[i]);
	}
	error_val = error_val >> 1;
      }
      state = SHOW_ERRORS;
    }
    break;
  case IDLE_DISPLAY:
    // this updates the important numbers each cycle
    {
      // the big number on the display
      // this is just ehz right now
      int x  = map(sine256[count], 0, 255, 0, 50);
      display.displayNum(x, 36, 30, 60);

      // display.displayNum(reportValues[2], 36, 30, 60);
      // miles
      display.displayNum(0, 9, (1 * LCD_WD / 3) + 8, (2 * LCD_HT / 3) + 18);
      // temp
      display.displayNum(reportValues[4], 9, 30, (2 * LCD_HT / 3) + 18);

      // update battery icon only when needed
      level = reportValues[2];
      if (abs(level - level_old) > .12) { display.updateBattery(level, MINVOLTAGE, MAXVOLTAGE); }
      level_old = level;
      
      display.updateCANErrorFlags(canFlag, errorFlag);

      int speed = (METRIC_PREF_ADDR) ? reportValues[1] * EHZ_SCALE_K : reportValues[1] * EHZ_SCALE_M;
      int pixelPos = scale(speed, 0, MAXSPEED, 0, 20);
      
      // foolin' around to play with pixels
      pixelPos = map(sine256[count], 0, 255, 0, 20);
      // update outer ring
      ring.clear();
      if (reportValues[0] < MAXSPEED) {
	// notice we add one to position
	ring.setPixelColor(1, colorArray[PIX_BLUE]);
	ring.setPixelColor(5, colorArray[PIX_BLUE]);
	ring.setPixelColor(9, colorArray[PIX_BLUE]);
	ring.setPixelColor(13, colorArray[PIX_BLUE]);
	ring.setPixelColor(17, colorArray[PIX_BLUE]);
	ring.setPixelColor(21, colorArray[PIX_BLUE]);
	ring.setPixelColor(1 + pixelPos, colorArray[pixelColor]);
      }
      else {
	ring.setPixelColor(19, PIX_RED); // warn the rider
	ring.setPixelColor(20, PIX_RED);
	ring.setPixelColor(21, PIX_RED);
      }
      ring.show(); 
    }
    break;
  case SHOW_ERRORS: // idle until timeout
  case IDLE_MENU:
    break;
  default:
    break;
  }

  delay(50);
  count += 1;
  if (count > 255) {count = 0;};
}

long scale(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updateRingBrightness() {
  for (int i=0; i<NUMPIXELS; i++) {
    ring.setBrightness(neoBrightVal);
    ring.setPixelColor(i, colorArray[pixelColor]);
  }
  ring.show(); 
}

void joystick() {
  for(int i = 0; i < btnMax; i++) {
    if (btnPressed[i] && elapsedBtnTime[i] > btnDelay) {
      elapsedBtnTime[i] = 0;
      btnPressed[i] = false;
      btnNum = i;

      switch (btnNum) {
      case 0: // back
	Serial.println("back"); 
	state = INIT_MENU;
	ms.back();
	break;
      case 1: // select. dont use the "pushdown" button
      case 3: // forward
	Serial.println("select"); 
	if (input_state == INPUT_MENU) {
	  state = INIT_MENU;
	  ms.select();
	}
	if (input_state == INPUT_NUMBER) { } // does nothing
	break;
      case 2: // up
	Serial.println("up");
	if (input_state == INPUT_MENU) {
	  state = INIT_MENU;
	  ms.prev();
	}
	if (input_state == INPUT_NUMBER) {
	  ms.touch();
	  neoBrightVal+=10;
	  if (neoBrightVal > 255) {neoBrightVal = 255;}
	  display.showBrightness(neoBrightVal);
	  updateRingBrightness();
	  EEPROM.write(PIX_BRIGHTNESS_ADDR, neoBrightVal);
	}
	break;
      case 4: // down
	Serial.println("down");
	if (input_state == INPUT_MENU) {
	  state = INIT_MENU;
	  ms.next();
	}
	if (input_state == INPUT_NUMBER) {
	  ms.touch();
	  int inc = 10;
	  if (neoBrightVal <= inc) {
	    inc = 2;
	  }
	  if (neoBrightVal <= inc) {
	    neoBrightVal = 2;
	  }
	  neoBrightVal -= inc;
	  display.showBrightness(neoBrightVal);
	  updateRingBrightness();
	  EEPROM.write(PIX_BRIGHTNESS_ADDR, neoBrightVal);
	}
	break;
      default:
	break;
      }
      delay(200);
    }
    if (digitalRead(btnPin[i]) == LOW && btnPressed[i] == false) {
      Serial.println("start");
      elapsedBtnTime[i] = 0;
      btnPressed[i] = true;
    }
    if (digitalRead(btnPin[i]) == HIGH) {
      btnPressed[i] = false;
    }
  }
}

// this is not CAN input. connect the USB from teensy to computer
//   and use the terminal to test menu. 
void serial_input() { 
  //  there are two menu collection states
  switch (input_state) {
  case INPUT_MENU:
    serial_menu_input();
    break;
  case INPUT_NUMBER:
    serial_brightness_input();
    break;
  default:
    break;
  }
}

/* 
****** keyboard input *******
* w: go to previus item (up)
* s: go to next item (down)
* a: go back (right)
* d: select item
***************
*/

void serial_menu_input() {
  char inChar;
  if((inChar = Serial.read())>0) {
    switch (inChar) {
    case 'w': // Previus item
      state = INIT_MENU;
      ms.prev();
      break;
    case 's': // Next item
      state = INIT_MENU;
      ms.next();
      break;
    case 'a': // Back pressed
      state = INIT_MENU;
      ms.back();
      break;
    case 'd': // Select pressed
      state = INIT_MENU;
      ms.select();
      break;
    default:
      break;
    }
  }
}

/* 
***************
* enable user to change a number
* w: increase number
* s: decrease
* a: go back
* d: do nothin'
***************
*/

void serial_brightness_input() {
  char inChar;
  if((inChar = Serial.read())>0) {
    switch (inChar) {
    case 'w': // Previus item
      Serial.println("ser: up");
      ms.touch();
      neoBrightVal+=10;
      if (neoBrightVal > 255) {neoBrightVal = 255;}
      display.showBrightness(neoBrightVal);
      updateRingBrightness();
      EEPROM.write(PIX_BRIGHTNESS_ADDR, neoBrightVal);
      break;
    case 's': // Next item
      lcd.fillScreen(BLACK);
      Serial.println("ser: down");
      ms.touch();
      neoBrightVal-=10;
      if (neoBrightVal <= 0) {neoBrightVal = 0;}
      display.showBrightness(neoBrightVal);
      updateRingBrightness();
      EEPROM.write(PIX_BRIGHTNESS_ADDR, neoBrightVal);
      break;
    case 'a': // Back pressed
      state = INIT_MENU;
      input_state = INPUT_MENU;
      ms.back();
      break;
    case 'd': // Select pressed
      break;
    case '?':

    default:
      break;
    }
  }
}

void can_input() {
  uint16_t packetId;
  union {
    uint8_t b[4];
    float f;
  } canData1;

  union {
    uint8_t b[4];
    float f;
  } canData2;

  if ( can.read(msg) ) {

    if (msg.len == CAN_PACKET_SIZE) {
      canData1.b[0]=msg.buf[0];
      canData1.b[1]=msg.buf[1];
      canData1.b[2]=msg.buf[2];
      canData1.b[3]=msg.buf[3];
      canData2.b[0]=msg.buf[4];
      canData2.b[1]=msg.buf[5];
      canData2.b[2]=msg.buf[6];
      canData2.b[3]=msg.buf[7];

      if (0) { // debug
	Serial.print("CAN "); 
	Serial.print("MB: "); Serial.print(msg.mb);
	Serial.print("  ID: 0x"); Serial.print(msg.id, HEX );
	Serial.print("  EXT: "); Serial.print(msg.flags.extended );
	Serial.print("  LEN: "); Serial.print(msg.len);
	Serial.print(" DATA: ");
	Serial.print("  TS: "); Serial.println(msg.timestamp);
      }

      packetId = (msg.id >> 16);

      switch (packetId) {
      case CAN_ID_SPEED:
	reportValues[1] = canData1.f;  // ehz
	canReceiveTime = millis();
	break;
      case CAN_ID_BUS_VOLT_CURR:
	reportValues[2] = canData1.f; // battery voltage
	reportValues[3] = canData2.f; // battery current
	reportValues[2] = (reportValues[2] < MINVOLTAGE) ? MINVOLTAGE : reportValues[2];
	canReceiveTime = millis();
	break;
      case CAN_ID_TEMP_MOT_MOS1:
	reportValues[4] = canData2.f;
	canReceiveTime = millis();
	break;
      default:
	break;
      }
    }
  }
}

void eepromDefaultValues() {
  uint8_t value; 
  
  // use this to initialize the teensy or if you create new things to store
  // main_display_state = val;
  if (RESET_STORED_VALUES) { 
    Serial.println("storing values:");
    EEPROM.write(PIX_BRIGHTNESS_ADDR, 125);
    EEPROM.write(PIX_COLOR_ADDR, 3);
    EEPROM.write(METRIC_PREF_ADDR, 0);
    EEPROM.write(DISPLAY_STATE_ADDR, 0);
  }

  Serial.println("getting stored values");
  value = EEPROM.read(PIX_BRIGHTNESS_ADDR);
  Serial.print(PIX_BRIGHTNESS_ADDR);
  Serial.print("\t");  Serial.print(value, DEC);  Serial.println();
  neoBrightVal = value;

  value = EEPROM.read(PIX_COLOR_ADDR);
  Serial.print(PIX_COLOR_ADDR);
  Serial.print("\t");  Serial.print(value, DEC);  Serial.println();
  pixelColor = value;

  value = EEPROM.read(METRIC_PREF_ADDR);
  Serial.print(METRIC_PREF_ADDR);
  Serial.print("\t");  Serial.print(value, DEC);  Serial.println();
  metric_pref = value;

  value = EEPROM.read(DISPLAY_STATE_ADDR);
  Serial.print(DISPLAY_STATE_ADDR);
  Serial.print("\t");  Serial.print(value, DEC);  Serial.println();
  main_display_state = value;
}
