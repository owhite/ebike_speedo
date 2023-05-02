#include <ST7789_t3.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>
#include "bitmaps.h" // converter is here https://www.cemetech.net/sc/


// LCD setup
#define LCD_SCLK 13  // SCL
#define LCD_MOSI 11  // SDA
#define LCD_CS   10  // CS
#define LCD_DC    9  // D/C
#define LCD_RST   8  // RST can use any pin
#define SCR_WD 160
#define SCR_HT 128
#define pgm_read_word(addr) (*(const unsigned short *)(addr))

ST7789_t3 lcd = ST7789_t3(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCLK, LCD_RST);

// Neopixel setup
#define PIXELPIN   7 
#define NUMPIXELS 24 
#define DELAYVAL 500 
int BRIGHTVAL = 5; 
Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// 43 x 32

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

void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      lcd.drawPixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
}


void setup(void) {
  Serial.begin(9600);
  lcd.initR(INITR_BLACKTAB); // for 128x160 display
  lcd.setRotation(3);

  // lcd.setCursor((SCR_WD / 2) - 40, SCR_HT / 2);
  // lcd.print("enjoy yourself");

  lcd.fillScreen(BLACK);

  ring.setBrightness(BRIGHTVAL);

  ring.begin();
  ring.clear();
  ring.show();
}

void loop() {
  // demoBig();
  int w=SCR_WD/3;

  drawRGBBitmap(0, 0, bitmap, BITMAP_WIDTH, BITMAP_HEIGHT);

  font.setColors(RGBto565(230,230,0),RGBto565(180,180,0),RGBto565(40,40,0));

  for(int i=0; i<ring.numPixels(); i++) { // For each pixel in strip...
    ring.clear();
    ring.setPixelColor(i, ring.Color(50,   50,   50)); 
    ring.show(); 
    delay(100);

    font.setSize1(w-20,SCR_HT/2,10);
    font.drawDigit2c(i%10,BITMAP_WIDTH,20);
  }
}

void demoBig()
{
  lcd.fillScreen(BLACK);
  int i=0;
  int w=SCR_WD/3;
  ms=millis();
  while(millis()-ms<6000) {
    font.setColors(RGBto565(230,230,0),RGBto565(180,180,0),RGBto565(40,40,0));
    font.setSize1(w-8,SCR_HT-20,10);
    font.drawDigit2c(i%10,0*w,0);

    font.setSize1(w-8,SCR_HT-20,12);
    font.setColors(RGBto565(230,230,0),RGBto565(180,180,0),RGBto565(40,40,0));
    font.drawDigit2c(i%10,1*w,0);

    font.setSize1(w-8,SCR_HT-20,17);
    font.setColors(RGBto565(230,230,0),RGBto565(180,180,0),RGBto565(40,40,0));
    font.drawDigit2c(i%10,2*w,0);
    delay(100);
    i++;
  }
}

