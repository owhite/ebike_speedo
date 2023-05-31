#include <MenuSystem.h>
#include "serial_setup.h"
#include "Arduino.h"
#include "states.h"

void serial_input(MenuSystem ms, int state, int input_state, int brightness) {
  switch (input_state) {
  case INPUT_MENU:
    serial_menu_input(ms, state, input_state);
    break;
  case INPUT_NUMBER:
    serial_brightness_input(ms, state, input_state, brightness);
    break;
  default:
    break;
  }
}

void serial_menu_input(MenuSystem ms, int state, int input_state) {
  char inChar;

  if (Serial.available() > 0) {

    inChar = Serial.read();

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
      Serial.println("BACK");
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


void serial_brightness_input(MenuSystem ms, int state, int input_state, int brightness) {
  char inChar;

  if (Serial.available() > 0) {
    inChar = Serial.read();
    switch (inChar) {
    case 'w': // Previus item
      brightness++;
      break;
    case 's': // Next item
      brightness--;
      break;
    case 'a': // Back pressed
      state = INIT_MENU;
      input_state = INPUT_MENU;
      ms.back();
      break;
    case 'd': // Select pressed
      break;
    case '?':

    default:
      break;
    }
  }
}
