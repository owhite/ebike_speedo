#include <Adafruit_NeoPixel.h>
#include <display_handling.h> 

// eeprom storage positions
#define RESET_STORED_VALUES 0x0000 // change to 1 if store new values
#define PIX_BRIGHTNESS_ADDR 0x0011
#define PIX_COLOR_ADDR      0x0012
#define METRIC_PREF_ADDR    0x0013

// Display things

displayHandler display;

int input_state = 0;
int state = 0;

// Neopixel setup
#define PIXELPIN   2
#define NUMPIXELS 24
Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

const uint8_t PIX_RED   = 0;
const uint8_t PIX_GREEN = 1;
const uint8_t PIX_BLUE  = 2;
const uint8_t PIX_WHITE = 3; 

uint32_t colorArray[] = {ring.Color(255, 0, 0), ring.Color(0, 255, 0), ring.Color(0, 0, 255), ring.Color(255, 255, 255)};

// gets changed later by getDefaultValues();
uint8_t metric_pref = 0;
uint8_t pixelColor = PIX_WHITE;
uint8_t neoBrightVal = 5;




