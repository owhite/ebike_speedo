// Libraries required to make this thing run.
//  respect all licensing associated with these libraries:
#include <CAN.h>
#include <MenuSystem.h> 
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>
// Other custom code blocks
#include <menu_handling.h>
#include <display_handling.h> 
#include <bitmaps.h>
#include <MESCerror.h>
#include <states.h>
#include <can_ids.h> // these will eventually be out of date 

// BIKE setup
#define BIKE_SPEED_MAX 50
#define POLEPAIRS    7
#define CIRCUMFERENCE 219.5 
#define GEAR_RATIO 9.82
#define CM_IN_MILE 160900
#define MINVOLTAGE 58
#define MAXVOLTAGE 75 
#define MAXTEMP 70
float EHZ_scale = (CIRCUMFERENCE * 60 * 60) / (CM_IN_MILE * GEAR_RATIO * POLEPAIRS);

// joystick button setup
const uint8_t btnMax = 5;
elapsedMillis elapsedBtnTime[btnMax] = {elapsedMillis(), elapsedMillis(), elapsedMillis(), elapsedMillis(), elapsedMillis()};
uint16_t btnDelay = 50;
boolean btnPressed[] = {false, false, false, false, false};
uint8_t btnPin[5] = {19, 21, 18, 16, 15};
uint8_t btnNum = 0;

// Round LCD setup
//  see Adafruit_GC9A01A github for pin usage
#define LCD_BL_PIN 20 // pin used for brightness
Adafruit_GC9A01A lcd(10, 9, 11, 13, 8, 12);

// CAN Variables
#define CAN_PACKET_SIZE  8
#define CAN_DEBUG        0
unsigned long canReceiveTime;
const unsigned long canInterval = 2000;
uint8_t canBuf[8] = {};
uint16_t packetId;

bool canFlag = false;
bool errorFlag = false;

union {
  uint8_t b[4];
  float f;
} canData1;

union {
  uint8_t b[4];
  float f;
} canData2;

// Digifont setup, set up here but used in displayHandlder object. 
void customLineH(int x0,int x1, int y, int c)    { lcd.drawFastHLine(x0,y,x1-x0+1,c); }
void customLineV(int x, int y0,int y1, int c)    { lcd.drawFastVLine(x,y0,y1-y0+1,c); } 
void customRect(int x, int y,int w,int h, int c) { lcd.fillRect(x,y,w,h,c); } 
DigiFont font(customLineH, customLineV, customRect);

displayHandler display;

// Display things
uint32_t dataInterval = 4000; 
int main_display_state = MAIN_DISPLAY_VOLTS;
int input_state = INPUT_MENU;
int state = IDLE_DISPLAY;
uint8_t brightness_val = 10;
int count = 0;
int pixelCount = 0;
bool entered_menu = true;
float level_old = 0.0;
float level = 0.8;

// Neopixel setup
// Neopixel setup
#define PIXELPIN   2
#define NUMPIXELS 24 
int BRIGHTVAL = 100; 
Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// Menu setup
MyRenderer my_renderer;
MenuSystem ms(my_renderer);

Menu       mu1    ("Display");
Menu       mu2    ("Errors");
MenuItem   mu3    ("Brightness",   [] { changeState(INIT_BRIGHTNESS); });
Menu       mu4    ("Speedo Colors");

MenuItem   mu1_mi1("Amps" , [] { updateMainDisplay(MAIN_DISPLAY_AMPS);  });
MenuItem   mu1_mi2("Volts", [] { updateMainDisplay(MAIN_DISPLAY_VOLTS); });
MenuItem   mu1_mi3("ehz"  , [] { updateMainDisplay(MAIN_DISPLAY_EHZ);   });
MenuItem   mu1_mi4("TEMP" , [] { updateMainDisplay(MAIN_DISPLAY_TEMP);  });
MenuItem   mu1_mi5("MPH"  , [] { updateMainDisplay(MAIN_DISPLAY_MPH);   });
MenuItem   mu1_mi6("KPH"  , [] { updateMainDisplay(MAIN_DISPLAY_KPH);   });
MenuItem   mu1_mi7("FW"   , [] { updateMainDisplay(MAIN_DISPLAY_FW);    });

MenuItem   mu2_mi1("View errors",  [] { changeState(INIT_ERRORS); });
MenuItem   mu2_mi2("Reset errors", &errorReset);

MenuItem   mu4_mi1("BLUE" , [] { changeLEDColor(0); });
MenuItem   mu4_mi2("WHITE", [] { changeLEDColor(1); });

// Menu callbacks
void changeLEDColor(int color) {
  Serial.print("LED color :: ");
  Serial.println( color );

  lcd.fillScreen(BLACK);
  lcd.setTextSize(2);
  lcd.setTextColor(WHITE);
  lcd.setCursor(20, 6 * LCD_CHAR_HEIGHT);
  lcd.print("change color:");
  lcd.setCursor(20, 8 * LCD_CHAR_HEIGHT);
  lcd.print(color);
  delay(1000); // pause to look at LCD

  display.setupFrame(main_display_state);
  entered_menu = false;
  state = IDLE_DISPLAY;
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
  display.setupFrame(main_display_state);
  entered_menu = false;
  state = IDLE_DISPLAY;
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

  pinMode(LCD_BL_PIN, OUTPUT); // LCD brightness
  // analogWrite(LCD_BL_PIN, 255);
  digitalWrite(LCD_BL_PIN, HIGH);

  lcd.begin();
  lcd.setRotation(0);
  lcd.fillScreen(BLACK);
  lcd.setTextSize(1);

  ring.setBrightness(BRIGHTVAL);
  ring.begin();
  ring.clear();
  ring.show();

  for (int i = 0; i < 5; i++) {
    pinMode(btnPin[i], INPUT);
  }

  // some parts parts of code change the LCD in the displayHandler
  //  while other parts do it here in main. not clever, but ¯\_(ツ)_/¯
  display.begin(&lcd, &font);

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
}

void loop() {
  int x;
  int y;
  int error_val = 10;

  serial_input();

  // menu timeout
  if ( (millis() - ms.get_last_menu_time() ) > dataInterval) { state = INIT_DISPLAY; }

  canFlag = false;
  if ((millis() - canReceiveTime ) > canInterval) {  
    canFlag = true;
  }

  // joystickHandler();

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
      display.updateBattery(level);
      entered_menu = false;
    }
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
    state = SET_BRIGHTNESS;
    }
    break;
  case INIT_ERRORS:
    {
    lcd.fillScreen(BLACK);
    lcd.setTextSize(2);
    y = 40;

    lcd.setCursor(30, y);
    lcd.setTextColor(WHITE);
    lcd.print("MESC ERRORS:");

    y += 2 * LCD_CHAR_HEIGHT;

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
    {
    // update the important numbers each cycle
    display.displayNum( sine256[count], 36, 30, 60);
    display.displayNum( 255 - sine256[count], 9, 30, (2 * LCD_HT / 3) + 18);
    display.displayNum( 255 - sine256[count], 9, (1 * LCD_WD / 3) + 8, (2 * LCD_HT / 3) + 18);

    // update battery icon only when needed
    level = 0.8;
    if (abs(level - level_old) > .02) { display.updateBattery(level); }
    level_old = level;
    display.updateCANErrorFlags(canFlag, errorFlag);

    // update outer ring
    ring.clear();
    ring.setPixelColor(pixelCount, ring.Color(50,   50,   50)); 
    ring.show(); 

    pixelCount++;
    if (pixelCount > NUMPIXELS) {
      pixelCount = 0;
    }

    }
    break;
  case SHOW_ERRORS: // idle until timeout
  case SET_BRIGHTNESS:
  case IDLE_MENU:
    break;
  default:
    break;
  }

  count++;
  if (count > 255) {count = 0;};
}

void joystickHandler() {
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
      case 1: // select
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
	  brightness_val++;
	  if (brightness_val > 255) {brightness_val = 255;}
	  display.showBrightness(brightness_val);
	}
	break;
      case 4: // down
	Serial.println("down");
	if (input_state == INPUT_MENU) {
	  state = INIT_MENU;
	  ms.next();
	}
	if (input_state == INPUT_NUMBER) {
	  Serial.println("down");
	  ms.touch();
	  brightness_val--;
	  if (brightness_val <= 0) {brightness_val = 0;}
	  display.showBrightness(brightness_val);
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
      Serial.println("up");
      ms.touch();
      brightness_val++;
      if (brightness_val > 255) {brightness_val = 255;}

      display.showBrightness(brightness_val);
      break;
    case 's': // Next item
      lcd.fillScreen(BLACK);
      Serial.println("down");
      ms.touch();
      brightness_val--;
      if (brightness_val <= 0) {brightness_val = 0;}

      display.showBrightness(brightness_val);
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

uint16_t extract_id(uint32_t ext_id) {
  return (ext_id >> 16);
}

void onReceive(int packetSize) {
  // received a packet
  packetId = extract_id(CAN.packetId());
  if (packetSize == CAN_PACKET_SIZE) {
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

    // hardcoded index into reportValues is kind of stupid
    // the idx corresponds items user wants
    // and map to: prefixes[] = {'M', 'E', 'B', 'A', 'T'}; 
    switch (packetId) {
    case CAN_ID_SPEED:
      // DEBUG_PRINT(" PACKET :: ");
      // DEBUG_PRINT(packetId);
      // reportValues[1] = canData1.f;  // this has ehz
      // reportValues[0] = reportValues[1] * EHZ_scale; // this has MPH
      // DEBUG_PRINT(" :: ");
      // DEBUG_PRINTLN(reportValues[0]);
      canReceiveTime = millis();
      break;
    case CAN_ID_BUS_VOLT_CURR:
      // reportValues[2] = canData1.f; // battery voltage
      // reportValues[3] = canData2.f; // battery current
      canReceiveTime = millis();
      break;
    case CAN_ID_TEMP_MOT_MOS1:
      // reportValues[4] = canData2.f;
      canReceiveTime = millis();
      break;
    default:
      break;
    }
  }
}

