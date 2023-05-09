#include <ST7789_t3.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>
#include "bitmaps.h" // converter is here https://www.cemetech.net/sc/
#include "MESCerror.h"

PROGMEM const int sine256[] = {127,130,133,136,139,143,146,149,152,155,158,161,164,167,170,173,176,178,181,184,187,190,192,195,198,200,203,205,208,210,212,215,217,219,221,223,225,227,229,231,233,234,236,238,239,240, 242,243,244,245,247,248,249,249,250,251,252,252,253,253,253,254,254,254,254,254,254,254,253,253,253,252,252,251,250,249,249,248,247,245,244,243,242,240,239,238,236,234,233,231,229,227,225,223, 221,219,217,215,212,210,208,205,203,200,198,195,192,190,187,184,181,178,176,173,170,167,164,161,158,155,152,149,146,143,139,136,133,130,127,124,121,118,115,111,108,105,102,99,96,93,90,87,84,81,78, 76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,37,35,33,31,29,27,25,23,21,20,18,16,15,14,12,11,10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,10,11,12,14,15,16,18,20,21,23,25,27,29,31, 33,35,37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,78,81,84,87,90,93,96,99,102,105,108,111,115,118,121,124 };

// LCD setup
#define LCD_SCLK 13  // SCL
#define LCD_MOSI 11  // SDA
#define LCD_CS   10  // CS
#define LCD_DC    9  // D/C
#define LCD_RST   8  // RST can use any pin
#define LCD_WD 160
#define LCD_HT 128
#define pgm_read_word(addr) (*(const unsigned short *)(addr))

ST7789_t3 lcd = ST7789_t3(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCLK, LCD_RST);

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
unsigned long ms;


int count = 0;

void setup(void) {
  Serial.begin(9600);
  lcd.initR(INITR_BLACKTAB); // for 128x160 display
  lcd.setRotation(3);

  setupFrame();
}

void loop() {

  for(int i=0; i<255; i++) {
    // num, size, x_pos, y_pos
    displayNum( sine256[i], 20, 0, 30);
    displayNum( 255 - sine256[i], 9, 0, (2 * LCD_HT / 3) + 18);
    displayNum( 255 - sine256[i], 9, (2 * LCD_WD / 3) + 8, (2 * LCD_HT / 3) + 18);
    delay(100);
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
	Serial.print(pos);
	Serial.print(" ");
	font.setColors(RGBto565(230,230,230),
		       RGBto565(180,180,180),
		       RGBto565(40,40,40));
	font.drawDigit2c(str.charAt(pos), x_pos + (font_space * j), y_pos );
      }
      else {
	Serial.print("X ");

	font.setColors(RGBto565(0,0,0),
		       RGBto565(0,0,0),
		       RGBto565(0,0,0));
	font.drawDigit2c(8, x_pos + (font_space * j), y_pos );
      }
      j++;
    }
  }
  Serial.println();
}

void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      lcd.drawPixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
}

