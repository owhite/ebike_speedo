/*
 * An example of a custom NumericMenuItem.
 * It tries to display some ASCII graphics in edit mode.
 * This can be useful if you want to give the end user an overview of the value limits.
 * 
 * Copyright (c) 2016 arduino-menusystem
 * Licensed under the MIT license (see LICENSE)
 */

#include <ST7789_t3.h>
#include <MenuSystem.h>

#define	BLACK   0x0000
#define PCD8544_CHAR_HEIGHT 8

#ifndef _MY_RENDERER_H
#define _MY_RENDERER_H

class MyRenderer : public MenuComponentRenderer {
public:
  void initLCD(ST7789_t3 *l);
  void render(Menu const& menu) const;
  void render_menu_item(MenuItem const& menu_item) const;
  void render_back_menu_item(BackMenuItem const& menu_item) const;
  void render_numeric_menu_item(NumericMenuItem const& menu_item) const;
  void render_menu(Menu const& menu) const;
  void menu_setup(Menu const& menu, MenuItem const& menu_item) const;

};

#endif
