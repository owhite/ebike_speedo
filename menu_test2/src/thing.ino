#include <MenuSystem.h>
#include "menu_handling.h"
#include <ST7789_t3.h>
#include <SPI.h>
#include <DigiFont.h>
#include "bitmaps.h" // converter is here https://www.cemetech.net/sc/
#include "MESCerror.h"
#include "states.h"

// LCD setup
#define LCD_SCLK 13  // SCL
#define LCD_MOSI 11  // SDA
#define LCD_CS   10  // CS
#define LCD_DC    9  // D/C
#define LCD_RST   8  // RST can use any pin
#define SCR_WD 160
#define SCR_HT 128
#define pgm_read_word(addr) (*(const unsigned short *)(addr))

#define PCD8544_CHAR_HEIGHT 8

ST7789_t3 lcd = ST7789_t3(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCLK, LCD_RST);

// Neopixel setup
#define PIXELPIN   7 
#define NUMPIXELS 24 
#define DELAYVAL 500 
int BRIGHTVAL = 5; 
// Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// DigiFont setup
#define RGBto565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define	GREY  RGBto565(128,128,128)
#define	LGREY RGBto565(160,160,160)
#define	DGREY RGBto565( 80, 80, 80)
#define	LBLUE RGBto565(100,100,255)
#define	DBLUE RGBto565(  0,  0,128)

void customLineH(int x0,int x1, int y, int c)    { lcd.drawFastHLine(x0,y,x1-x0+1,c); }
void customLineV(int x, int y0,int y1, int c)    { lcd.drawFastVLine(x,y0,y1-y0+1,c); } 
void customRect(int x, int y,int w,int h, int c) { lcd.fillRect(x,y,w,h,c); } 
DigiFont font(customLineH, customLineV, customRect);

int state = IDLE;
// Menu setup

MyRenderer my_renderer;
MenuSystem ms(my_renderer);

// Menu callbacks
void changeState(int val) {
  Serial.print("state change :: ");
  Serial.println( val );

  lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
  lcd.print("state change: ");
  lcd.print(val);
  delay(500); // pause to look at LCD

  state = val;
}

void on_item2_selected(MenuComponent* p_menu_component) {
  lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
  lcd.print("Item2 Selectd");
  Serial.println("Item2 Selectd");
  delay(1500); // so we can look the result on the LCD
}

void on_item3_selected(MenuComponent* p_menu_component) {
  lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
  lcd.print("3 Selected, IDLING");
  Serial.println("Item3 Selected");

  state = DISPLAY_VOLTS;

  delay(500); 
}

Menu       mu1    ("Display");
Menu       mu2    ("Errors");
Menu       mu3    ("Set variables");

MenuItem   mu1_mi1("Amps" , [] { changeState(DISPLAY_AMPS);  });
MenuItem   mu1_mi2("Volts", [] { changeState(DISPLAY_VOLTS); });
MenuItem   mu1_mi3("ehz"  , [] { changeState(DISPLAY_EHZ);   });
MenuItem   mu1_mi4("MPH"  , [] { changeState(DISPLAY_MPH);   });
MenuItem   mu1_mi5("TEMP" , [] { changeState(DISPLAY_TEMP);  });

MenuItem   mu2_mi1("View errors",  &on_item2_selected);
MenuItem   mu2_mi2("Reset errors", &on_item2_selected);

MenuItem   mu3_mi1("Field weakening", &on_item3_selected);
MenuItem   mu3_mi2("Max power",       &on_item3_selected);
MenuItem   mu3_mi3("Max current",     &on_item3_selected);

void setup() {
  Serial.begin(9600);
  lcd.initR(INITR_BLACKTAB); // for 128x160 display
  lcd.setRotation(3);
  lcd.fillScreen(BLACK);
  lcd.setTextSize(1);

  my_renderer.initLCD(&lcd);
  ms.get_root_menu().add_menu(&mu1);
  mu1.add_item(&mu1_mi1);
  mu1.add_item(&mu1_mi2);
  mu1.add_item(&mu1_mi3);
  mu1.add_item(&mu1_mi4);

  ms.get_root_menu().add_menu(&mu2);
  mu2.add_item(&mu2_mi1);
  mu2.add_item(&mu2_mi2);

  ms.get_root_menu().add_menu(&mu3);
  mu3.add_item(&mu3_mi1);
  mu3.add_item(&mu3_mi2);
  mu3.add_item(&mu3_mi3);

  ms.display();
}

void loop() {
  serial_handler();

  switch (state) {
  case INIT_MENU:
    ms.display();
    state = SHOW_MENU;
    break;
  case SHOW_MENU:
    break;
  case DISPLAY_AMPS:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("amps");
    state = IDLE;
    break;
  case DISPLAY_VOLTS:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("volts");
    state = IDLE;
    break;
  case DISPLAY_EHZ:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("ehz");
    state = IDLE;
    break;
  case DISPLAY_MPH:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("mph");
    state = IDLE;
    break;
  case DISPLAY_TEMP:
    lcd.fillScreen(BLACK);
    lcd.setCursor(0, 10);
    lcd.print("temps");
    state = IDLE;
    break;
  case IDLE:
    break;
  default:
    break;
  }
}

void serial_handler() {
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