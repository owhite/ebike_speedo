#ifndef SERIAL_SETUP_H
#define SERIAL_SETUP_H

#include "states.h"
#include <MenuSystem.h>

void serial_input(MenuSystem ms, int state, int input_state, int brightness);
void serial_menu_input(MenuSystem ms, int state, int input_state);
void serial_brightness_input(MenuSystem ms, int state, int input_state, int brightness);


#endif
