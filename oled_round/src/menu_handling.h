#include "Adafruit_GC9A01A.h"
#include <MenuSystem.h>

// didnt get these to work
#define	RENDER_LIST    0
#define	RENDER_BOTTOM  1

#define	BLACK   0x0000
#define LCD_TEXT_SIZE 2
#define LCD_CHAR_HEIGHT LCD_TEXT_SIZE * 8
#define LCD_CHAR_WIDTH  LCD_TEXT_SIZE * 7

#ifndef _MY_RENDERER_H
#define _MY_RENDERER_H

class MyRenderer : public MenuComponentRenderer {
public:
  void initLCD(Adafruit_GC9A01A *l);
  void render(Menu const& menu) const;
  void render_menu_item(MenuItem const& menu_item) const;
  void render_back_menu_item(BackMenuItem const& menu_item) const;
  void render_numeric_menu_item(NumericMenuItem const& menu_item) const;
  void render_menu(Menu const& menu) const;
  void menu_setup(Menu const& menu, MenuItem const& menu_item) const;

private:
  Adafruit_GC9A01A *_lcd;

};

#endif
