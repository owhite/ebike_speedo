#include "menu_handling.h"
#include "states.h"

ST7789_t3 *_lcd;

void MyRenderer::initLCD(ST7789_t3 *l) {
  _lcd = l;
}

void MyRenderer::render(Menu const& menu) const {
  // Serial.print("\nCurrent menu name:\n");
  _lcd->fillScreen(BLACK);

  Serial.print("TYPE: ");

  Serial.println(menu.get_type());

  int x = 4;
  int y = LCD_CHAR_HEIGHT;

  for (int i = 0; i < menu.get_num_components(); ++i) {
    MenuComponent const* cp_m_comp = menu.get_menu_component(i);
    cp_m_comp->render(*this);
    Serial.print(cp_m_comp->get_name());

    _lcd->setCursor(x, y);
    if (cp_m_comp->is_current()) {

      size_t length = strlen(cp_m_comp->get_name());

      _lcd->drawRect(x-2, y - 2, strlen(cp_m_comp->get_name()) * 7, LCD_CHAR_HEIGHT + 4, ST7735_WHITE);
      _lcd->print(cp_m_comp->get_name());
      Serial.print("<<< ");
    }
    else {
      _lcd->print(cp_m_comp->get_name());
    }

    y += LCD_CHAR_HEIGHT + 7;
    Serial.println();
  }
}

void MyRenderer::render_menu_item(MenuItem const& menu_item) const {
  // Serial.print(menu_item.get_name());
  // lastMenuMS = millis();
}

void MyRenderer::render_back_menu_item(BackMenuItem const& menu_item) const {
  // Serial.print(menu_item.get_name());
  /// lastMenuMS = millis();
}

void MyRenderer::render_menu(Menu const& menu) const {
  // Serial.print(menu.get_name());
  // lastMenuMS = millis();
}

void MyRenderer::render_numeric_menu_item(NumericMenuItem const& menu_item) const {
  // lastMenuMS = millis();
}

void MyRenderer::menu_setup(Menu const& menu, MenuItem const& menu_item) const {
  // lastMenuMS = millis();
}

