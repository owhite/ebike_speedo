#include "Arduino.h"
#include "display_handling.h"
#include "bitmaps.h" 

displayHandler::displayHandler() {
}

void displayHandler::begin(Adafruit_GC9A01A *l, DigiFont *f, Adafruit_NeoPixel *r) {
    _lcd = l;
    _font = f;
    _ring = r;
}

int displayHandler::starRandom(int lower, int upper) { return lower + static_cast<int>(rand() % (upper - lower + 1)); }

void displayHandler::starDisplay() {
  int origin_x = LCD_WD/2;
  int origin_y = LCD_HT/2;

  // Iterate through the stars reducing the z co-ordinate in order to move the
  // star closer.
  for (int i = 0; i < _starCount; ++i) {
    _stars[i][2] -= 0.4;
    // if the star has moved past the screen (z < 0) reposition it far away
    // with random x and y positions.
    if (_stars[i][2] <= 0) {
      _stars[i][0] = displayHandler::starRandom(-25, 25);
      _stars[i][1] = displayHandler::starRandom(-25, 25);
      _stars[i][2] = _maxDepth;
    }

    // Convert the 3D coordinates to 2D using perspective projection.
    double k = LCD_WD / _stars[i][2];
    int x = static_cast<int>(_stars[i][0] * k + origin_x);
    int y = static_cast<int>(_stars[i][1] * k + origin_y);

    //  Draw the star (if it is visible in the screen).
    // Distant stars are smaller than closer stars.
    if ((0 <= x and x < LCD_WD) 
	and (0 <= y and y < LCD_HT)) {
      int size = (1 - _stars[i][2] / _maxDepth) * 4;
      _lcd->fillRect(x,y,size,size,WHITE);
    }
  }
}

void displayHandler::showStarField() {
  // Initialise the star field with random stars
  for (int i = 0; i < _starCount; i++) {
    _stars[i][0] = displayHandler::starRandom(-25, 25);
    _stars[i][1] = displayHandler::starRandom(-25, 25);
    _stars[i][2] = displayHandler::starRandom(0, _maxDepth);
  }
  
  for(int i = 0; i<100; i++) {
    starDisplay();
    _ring->clear();
    _ring->setPixelColor(i % 24, _ring->Color(0, 255, 0));
    _ring->show(); 
    delay(50);
  }
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

void displayHandler::updateBattery(float level, float min, float max) {
  uint8_t battery_x = (LCD_WD / 2) + 20;
  uint8_t battery_y = 30;

  uint16_t color1 = GREEN;
  uint16_t color2 = RED;
  uint8_t b_width = 65;
  uint8_t tab_width = 6;
  uint8_t b_height = 30;
  uint8_t tab_height = 20;

  level = (level < min) ? min : level;
  level = map(level, min, max, 0, 1);

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
  _lcd->print("brightness:");
  _lcd->setCursor(20, 8 * LCD_CHAR_HEIGHT);
  _lcd->print("(max 250)");
  _lcd->setCursor(20, 10 * LCD_CHAR_HEIGHT);
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


  if (_canFlag_old != canFlag || _errorFlag_old != errorFlag) {

    _lcd->fillRect(panel2_x + 6, panel1_y + 6, CAN_BITMAP_WIDTH + 4, CAN_BITMAP_HEIGHT * 4, BLACK);

    if (errorFlag) {
      displayHandler::drawRGBBitmap(panel2_x + 8, panel1_y+8, error_bitmap, ERROR_BITMAP_WIDTH, ERROR_BITMAP_HEIGHT);
    }
    else if (canFlag) {
      displayHandler::drawRGBBitmap(panel2_x + 8, panel1_y+8, can_bitmap, CAN_BITMAP_WIDTH, CAN_BITMAP_HEIGHT);
    }
    else {
      _lcd->fillCircle(panel2_x + 12 + CAN_BITMAP_WIDTH/2, panel1_y+24, 12, GREEN);
    }
    _canFlag_old = canFlag;
    _errorFlag_old = errorFlag;
  }

}


void displayHandler::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      _lcd->drawPixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
}

