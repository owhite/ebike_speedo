#include <Adafruit_NeoPixel.h>

#define PIXEL_PIN 7 // Digital IO pin connected to the NeoPixels.
#define PIXEL_COUNT 14 // Number of NeoPixels

int BRIGHTVAL = 0; // Variable to store birghness value in 

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

boolean oldState = HIGH;
int mode = 0; // Currently-active animation mode, 0-9

void setup() {
 strip.begin(); // Initialize NeoPixel strip object (REQUIRED)
 strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  colorWipe(strip.Color(255,   0,   0), 50);    // Red
}

void colorWipe(uint32_t color, int wait) {
 for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
 strip.setPixelColor(i, color); // Set pixel's color (in RAM)
 strip.setBrightness(BRIGHTVAL); // Set brightness value between 0-255
 strip.show(); // Update strip to match
 delay(wait); // Pause for a moment
 }
}
