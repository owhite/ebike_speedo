#include "Arduino.h"
#include "display_handling.h"
#include "bitmaps.h" 

displayHandler::displayHandler() {
}

void displayHandler::begin(Adafruit_GC9A01A *l,   DigiFont *f) {
    _lcd = l;
    _font = f;
}

// right justifies digit uses the hideous arduino String() function
void displayHandler::displayNum(int n, int font_width, int x_pos, int y_pos) {
  String str = String(n);

  int font_space = font_width * 1.8;
  _font->setSize1(font_width, font_width * 2, font_width / 2.4);

  if (str.length() < 4) {
    int j = 0;
    for(int i=3; i > 0; i--) {

      int pos = str.length() - i;

      if (pos >= 0) {
        _font->setColors(RGBto565(230,230,230),
                       RGBto565(180,180,180),
                       RGBto565(0,0,0));
        _font->drawDigit2c(str.charAt(pos), x_pos + (font_space * j), y_pos );
      }
      else {
        _font->setColors(RGBto565(0,0,0),
                       RGBto565(0,0,0),
                       RGBto565(0,0,0));
        _font->drawDigit2c(8, x_pos + (font_space * j), y_pos );
      }
      j++;
    }
  }

}

void displayHandler::updateBattery(float level) {
  uint8_t battery_x = (LCD_WD / 2) + 20;
  uint8_t battery_y = 30;

  uint16_t color1 = GREEN;
  uint16_t color2 = RED;
  uint8_t b_width = 65;
  uint8_t tab_width = 6;
  uint8_t b_height = 30;
  uint8_t tab_height = 20;

  _lcd->fillRect(battery_x-(b_width/2),
	       battery_y-(b_height/2),
	       b_width, b_height, color1);

  _lcd->fillRect(battery_x-(b_width/2)-tab_width, 
	       battery_y - (tab_height / 2),
  	       tab_width, tab_height, color1);

  uint8_t pad = 3;
  uint8_t x1 = battery_x-(b_width/2) + pad;
  uint8_t y1 = battery_y-(b_height/2) + pad;
  uint8_t x2 = b_width - (2 * pad);
  uint8_t y2 = b_height - (2 * pad);
  _lcd->drawRect(x1, y1, x2, y2, color2);

  x2 = x2 * (1 - level);

  _lcd->fillRect(x1, y1, x2, y2, color2);

  _lcd->setTextSize(4);
  _lcd->setCursor(battery_x + (b_width/2), battery_y - 12);
  _lcd->setTextColor(BLACK);
  if (level < 0.2) {
    _lcd->setTextColor(WHITE);
  }
  _lcd->print("!");
}

void displayHandler::showBrightness(uint8_t val) {
  _lcd->fillScreen(BLACK);
  _lcd->setTextSize(2);
  _lcd->setTextColor(WHITE);
  _lcd->setCursor(20, 6 * LCD_CHAR_HEIGHT);
  _lcd->print("brightness: ");
  _lcd->setCursor(20, 8 * LCD_CHAR_HEIGHT);
  _lcd->print(val);
}

void displayHandler::setupFrame(int display_state) {
  _lcd->fillScreen(BLACK);
  uint8_t panel1_y = 2 * LCD_HT / 3;
  uint8_t panel1_x = 1 * LCD_WD / 3;
  uint8_t panel2_x = 2 * LCD_WD / 3;

  // left to right separator
  _lcd->fillRect(0, panel1_y - 2, LCD_WD, 6, LGREY);
  _lcd->fillRect(0, panel1_y, LCD_WD, 2, DGREY);

  // bottom separators
  _lcd->fillRect(panel1_x,   panel1_y+2, 6, LCD_HT - panel1_y + 2, LGREY);
  _lcd->fillRect(panel1_x+1, panel1_y, 4, LCD_HT - panel1_y + 2, DGREY);
  _lcd->fillRect(panel2_x,   panel1_y+2, 6, LCD_HT - panel1_y + 2, LGREY);
  _lcd->fillRect(panel2_x+1, panel1_y, 4, LCD_HT - panel1_y + 2, DGREY);

  // top row title
  displayHandler::drawRGBBitmap(32, 34, bitmap[display_state], BITMAP_WIDTH, BITMAP_HEIGHT);
  // lower left title
  displayHandler::drawRGBBitmap(20, panel1_y+4, trip, TRIP_WIDTH, TRIP_HEIGHT);
  // lower right title
  displayHandler::drawRGBBitmap(panel1_x+6, panel1_y+4, temp, TEMP_WIDTH, TEMP_HEIGHT);

}

// update CAN or error flags
void displayHandler::updateCANErrorFlags(bool canFlag, bool errorFlag) {
  uint8_t panel1_y = 2 * LCD_HT / 3;
  uint8_t panel2_x = 2 * LCD_WD / 3;

  if (canFlag) {
    displayHandler::drawRGBBitmap(panel2_x + 8, panel1_y+8, can_bitmap, CAN_BITMAP_WIDTH, CAN_BITMAP_HEIGHT);
    return;
  }
  else if (errorFlag) {
    displayHandler::drawRGBBitmap(panel2_x + 8, panel1_y+8, error_bitmap, ERROR_BITMAP_WIDTH, ERROR_BITMAP_HEIGHT);
  }
  else {
    _lcd->fillRect(panel2_x + 8, panel1_y+8, CAN_BITMAP_WIDTH, CAN_BITMAP_HEIGHT, BLACK);
  }
}


void displayHandler::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      _lcd->drawPixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
}

