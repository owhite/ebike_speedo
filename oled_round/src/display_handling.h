#include "Arduino.h"
#include <Adafruit_GC9A01A.h>
#include <Adafruit_NeoPixel.h>
#include <DigiFont.h>

#ifndef display_handling_h
#define display_handling_h

#define LCD_WD 240
#define LCD_HT 240
#define LCD_CHAR_HEIGHT 12

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define RGBto565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define GREY  RGBto565(128,128,128)
#define LGREY RGBto565(160,160,160)
#define DGREY RGBto565( 80, 80, 80)
#define LBLUE RGBto565(100,100,255)
#define DBLUE RGBto565(  0,  0,128)

#define pgm_read_word(addr) (*(const unsigned short *)(addr))

// star field stuff
class displayHandler
{
  public:

  displayHandler();
  void begin(Adafruit_GC9A01A *l, DigiFont *f, Adafruit_NeoPixel *ring);
  void updateCANErrorFlags(bool canFlag, bool errorFlag);
  void updateBattery(float level, float min, float max);
  void displayNum(int n, int font_width, int x_pos, int y_pos);
  void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h);
  void showBrightness(uint8_t val);
  void setupFrame(int display_state);
  int starRandom(int lower, int upper);
  void starDisplay();
  void showStarField();


  private:
  bool _canFlag_old;
  bool _errorFlag_old;
  int _pin;
  Adafruit_GC9A01A *_lcd;
  DigiFont *_font;
  Adafruit_NeoPixel *_ring;
  constexpr static  int _starCount = 512;
  int _maxDepth = 32; 
  double _stars[_starCount][3]; // x, y and z co-ordinates
};

#endif
