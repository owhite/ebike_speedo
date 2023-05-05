#include "menu_handling.h"
#include "states.h"

ST7789_t3 *_lcd;

void MyRenderer::initLCD(ST7789_t3 *l) {
  _lcd=l;
}

void MyRenderer::render(Menu const& menu) const {
  Serial.print("\nCurrent menu name: ");
  Serial.println(menu.get_name());

  _lcd->fillScreen(BLACK);

  for (int i = 0; i < menu.get_num_components(); ++i) {
    MenuComponent const* cp_m_comp = menu.get_menu_component(i);
    cp_m_comp->render(*this);
    Serial.print(cp_m_comp->get_name());
    _lcd->setCursor(0, (i+1) * PCD8544_CHAR_HEIGHT);
    _lcd->print(cp_m_comp->get_name());
    if (cp_m_comp->is_current()) {
      _lcd->print(" X ");
      Serial.print("<<< ");
    }
    Serial.println();
  }
}

void MyRenderer::render_menu_item(MenuItem const& menu_item) const {
  Serial.print(menu_item.get_name());
}

void MyRenderer::render_back_menu_item(BackMenuItem const& menu_item) const {
  Serial.print(menu_item.get_name());
}

void MyRenderer::render_menu(Menu const& menu) const {
  Serial.print(menu.get_name());
}

void MyRenderer::render_numeric_menu_item(NumericMenuItem const& menu_item) const {
}

void MyRenderer::menu_setup(Menu const& menu, MenuItem const& menu_item) const {

}

