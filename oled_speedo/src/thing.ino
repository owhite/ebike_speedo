#include <MenuSystem.h>
#include "menu_handling.h"
#include <ST7789_t3.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>
#include "bitmaps.h" // converter is here https://www.cemetech.net/sc/
#include "MESCerror.h"
#include "states.h"

PROGMEM const int sine256[] = {127,130,133,136,139,143,146,149,152,155,158,161,164,167,170,173,176,178,181,184,187,190,192,195,198,200,203,205,208,210,212,215,217,219,221,223,225,227,229,231,233,234,236,238,239,240, 242,243,244,245,247,248,249,249,250,251,252,252,253,253,253,254,254,254,254,254,254,254,253,253,253,252,252,251,250,249,249,248,247,245,244,243,242,240,239,238,236,234,233,231,229,227,225,223, 221,219,217,215,212,210,208,205,203,200,198,195,192,190,187,184,181,178,176,173,170,167,164,161,158,155,152,149,146,143,139,136,133,130,127,124,121,118,115,111,108,105,102,99,96,93,90,87,84,81,78, 76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,37,35,33,31,29,27,25,23,21,20,18,16,15,14,12,11,10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,10,11,12,14,15,16,18,20,21,23,25,27,29,31, 33,35,37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,78,81,84,87,90,93,96,99,102,105,108,111,115,118,121,124 };

// BIKE setup
#define BIKE_SPEED_MAX 50

// LCD setup
#define LCD_SCLK 13  // SCL
#define LCD_MOSI 11  // SDA
#define LCD_CS   10  // CS
#define LCD_DC    9  // D/C
#define LCD_RST   8  // RST can use any pin
#define LCD_WD 160
#define LCD_HT 128
#define pgm_read_word(addr) (*(const unsigned short *)(addr))

#define LCD_CHAR_HEIGHT 12

ST7789_t3 lcd = ST7789_t3(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCLK, LCD_RST);

// DigiFont setup
#define RGBto565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define GREY  RGBto565(128,128,128)
#define LGREY RGBto565(160,160,160)
#define DGREY RGBto565( 80, 80, 80)
#define LBLUE RGBto565(100,100,255)
#define DBLUE RGBto565(  0,  0,128)

void customLineH(int x0,int x1, int y, int c)    { lcd.drawFastHLine(x0,y,x1-x0+1,c); }
void customLineV(int x, int y0,int y1, int c)    { lcd.drawFastVLine(x,y0,y1-y0+1,c); } 
void customRect(int x, int y,int w,int h, int c) { lcd.fillRect(x,y,w,h,c); } 
DigiFont font(customLineH, customLineV, customRect);

// LCD timing
boolean showData = false; // data as in MPH, ehz, Amps, temp
uint32_t dataPrev = 0;
uint32_t dataInterval = 4000; 
uint32_t dataNow;

// Neopixel setup
#define PIXELPIN   7 
#define NUMPIXELS 24 
int BRIGHTVAL = 5; 
Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

int input_state = INPUT_MENU;
int state = IDLE_DISPLAY;
int brightness_val = 100;
int count = 0;
int pixelCount = 0;
bool screen_was_reset = false;

MyRenderer my_renderer;
MenuSystem ms(my_renderer);

// Menu callbacks
void changeState(int val) {
  Serial.print("state change :: ");
  Serial.println( val );

  lcd.setCursor(0, 6 * LCD_CHAR_HEIGHT);
  lcd.print("state change: ");
  lcd.print(val);
  delay(500); // pause to look at LCD

  state = val;
}

// doesnt do anything yet
void errorReset(MenuComponent* p_menu_component) {
  lcd.setCursor(0, 6 * LCD_CHAR_HEIGHT);
  lcd.print("Error codes reset");
  Serial.println("Error reset selected");
  state = IDLE_MENU;
}

Menu       mu1    ("Display");
Menu       mu2    ("Errors");
MenuItem   mu3    ("Brightness",   [] { changeState(INIT_BRIGHTNESS); });

MenuItem   mu1_mi1("Amps" , [] { changeState(DISPLAY_AMPS);  });
MenuItem   mu1_mi2("Volts", [] { changeState(DISPLAY_VOLTS); });
MenuItem   mu1_mi3("ehz"  , [] { changeState(DISPLAY_EHZ);   });
MenuItem   mu1_mi4("MPH"  , [] { changeState(DISPLAY_MPH);   });
MenuItem   mu1_mi5("TEMP" , [] { changeState(DISPLAY_TEMP);  });

MenuItem   mu2_mi1("View errors",  [] { changeState(DISPLAY_ERRORS); });
MenuItem   mu2_mi2("Reset errors", &errorReset);

void setup() {
  Serial.begin(9600);
  lcd.initR(INITR_BLACKTAB); // for 128x160 display
  lcd.setRotation(3);
  lcd.fillScreen(BLACK);
  lcd.setTextSize(1);

  ring.setBrightness(BRIGHTVAL);
  ring.begin();
  ring.clear();
  ring.show();

  my_renderer.initLCD(&lcd);
  ms.get_root_menu().add_menu(&mu1, RENDER_LIST);
  mu1.add_item(&mu1_mi1);
  mu1.add_item(&mu1_mi2);
  mu1.add_item(&mu1_mi3);
  mu1.add_item(&mu1_mi4);

  ms.get_root_menu().add_menu(&mu2, RENDER_BOTTOM);
  mu2.add_item(&mu2_mi1);
  mu2.add_item(&mu2_mi2);

  ms.get_root_menu().add_item(&mu3);

  setupFrame();
}

void loop() {

  serial_input();

  if ( (millis() - ms.get_last_menu_time() ) > dataInterval) {
    if (! screen_was_reset) { setupFrame(); }
    state = IDLE_DISPLAY;
  }

  switch (state) {
  case INIT_MENU:
    screen_was_reset = false;
    ms.display();
    state = SHOW_MENU;
    break;
  case INIT_BRIGHTNESS:
    input_state = INPUT_NUMBER;
    state = SET_BRIGHTNESS;
    lcd.fillScreen(BLACK);
    break;
  case SET_BRIGHTNESS:
    lcd.setCursor(0, 10);
    lcd.print("brightness");
    lcd.setCursor(0, 20);
    lcd.print(brightness_val);
    state = SET_BRIGHTNESS;
    break;
  case IDLE_DISPLAY:
    displayNum( sine256[count], 20, 0, 30);
    displayNum( 255 - sine256[count], 9, 0, (2 * LCD_HT / 3) + 18);
    displayNum( 255 - sine256[count], 9, (2 * LCD_WD / 3) + 8, (2 * LCD_HT / 3) + 18);

    
    int range = ring.numPixels() - 5;
    int x = map(sine256[count], 0, 255, 0, BIKE_SPEED_MAX);
    int y = map(x, 0, BIKE_SPEED_MAX, 0, range);
    ring.clear();
    ring.setPixelColor(y, ring.Color(50,   50,   50)); 
    ring.show(); 

    break;
  case DISPLAY_ERRORS:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("MESC ERRORS:");
    Serial.println("errors...");
    int error_val = 10;
    int count = 2;
    for(int i=0;i<32;i++) {
      if (error_val & 0x0001) {
	lcd.setCursor(8, count * LCD_CHAR_HEIGHT);
	count++;
	lcd.print(error_codes[i]);
	Serial.println(error_codes[i]);
      }
      error_val = error_val >> 1;
    }
    state = IDLE_MENU;
    break;
  case SHOW_MENU:
  case IDLE_MENU:
    break;
  default:
    break;
  }

  pixelCount++;
  if (pixelCount > BIKE_SPEED_MAX) { pixelCount = 0; }

  count++;
  if (count > 255) {count = 0;};
}

void serial_input() {
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
    case '?':
    case 'h': // Display help
      Serial.println("***************");
      Serial.println("w: go to previus item (up)");
      Serial.println("s: go to next item (down)");
      Serial.println("a: go back (right)");
      Serial.println("d: select \"selected\" item");
      Serial.println("?: print this help");
      Serial.println("h: print this help");
      Serial.println("***************");
      break;
    default:
      break;
    }
  }
}


void serial_brightness_input() {
  char inChar;
  if((inChar = Serial.read())>0) {
    switch (inChar) {
    case 'w': // Previus item
      lcd.fillScreen(BLACK);
      brightness_val++;
      break;
    case 's': // Next item
      lcd.fillScreen(BLACK);
      brightness_val--;
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

void setupFrame() {
  lcd.fillScreen(BLACK);
  uint8_t panel1_y = 2 * LCD_HT / 3;
  uint8_t panel3_x = 1 * LCD_WD / 3;
  uint8_t panel4_x = 2 * LCD_WD / 3;

  // top / bottom separator
  lcd.fillRect(0, panel1_y - 2, LCD_WD, 6, LGREY);
  lcd.fillRect(0, panel1_y, LCD_WD, 2, DGREY);

  // top / bottom separator
  lcd.fillRect(panel3_x,   panel1_y+2, 6, LCD_HT - panel1_y + 2, LGREY);
  lcd.fillRect(panel3_x+1, panel1_y, 4, LCD_HT - panel1_y + 2, DGREY);

  lcd.fillRect(panel4_x,   panel1_y+2, 6, LCD_HT - panel1_y + 2, LGREY);
  lcd.fillRect(panel4_x+1, panel1_y, 4, LCD_HT - panel1_y + 2, DGREY);

  // top row number
  drawRGBBitmap(0, 0, bitmap, BITMAP_WIDTH, BITMAP_HEIGHT);

  // lower left number
  drawRGBBitmap(0, panel1_y+4, trip, TRIP_WIDTH, TRIP_HEIGHT);

  // lower right number
  drawRGBBitmap(panel4_x+6, panel1_y+4, temp, TEMP_WIDTH, TEMP_HEIGHT);

  screen_was_reset = true;
}

// right justifies digit uses the hideous arduino String() function
void displayNum(int n, int font_width, int x_pos, int y_pos) {
  String str = String(n);

  int font_space = font_width * 1.8;
  font.setSize1(font_width, font_width * 2, font_width / 2.4);

  if (str.length() < 4) {
    int j = 0;
    for(int i=3;i > 0; i--) {

      int pos = str.length() - i;

      if (pos >= 0) {
        font.setColors(RGBto565(230,230,230),
                       RGBto565(180,180,180),
                       RGBto565(40,40,40));
        font.drawDigit2c(str.charAt(pos), x_pos + (font_space * j), y_pos );
      }
      else {
        font.setColors(RGBto565(0,0,0),
                       RGBto565(0,0,0),
                       RGBto565(0,0,0));
        font.drawDigit2c(8, x_pos + (font_space * j), y_pos );
      }
      j++;
    }
  }
}

void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      lcd.drawPixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
}
