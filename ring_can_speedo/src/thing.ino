#include <ST7735_t3.h>    // Hardware-specific library
#include <ST7789_t3.h>    // Hardware-specific library
#include <SPI.h>

#define TFT_SCLK 13  // SCL
#define TFT_MOSI 11  // SDA
#define TFT_CS   10  // CS
#define TFT_DC    9  // D/C
#define TFT_RST   8  // RST can use any pin

// Use one or the other
ST7789_t3 tft = ST7789_t3(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

int width = 160;
int height = 128;

void setup(void) {
  Serial.begin(9600);
  tft.initR(INITR_BLACKTAB); // if you're using a 1.8" TFT 128x160 displays
  tft.setRotation(3);

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor((width / 2) - 40, height / 2);
  tft.print("enjoy yourself");
  tft.drawRect(1, 1, width - 2,height - 2, ST77XX_YELLOW);
}

void loop() {
  Serial.print(tft.width());
  Serial.print(" :: ");
  Serial.println(tft.height());
}


