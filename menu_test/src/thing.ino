#include <MenuSystem.h>
#include <ST7789_t3.h>
#include <SPI.h>
#include <DigiFont.h>
#include "bitmaps.h" // converter is here https://www.cemetech.net/sc/
#include "MESCerror.h"

// LCD setup
#define LCD_SCLK 13  // SCL
#define LCD_MOSI 11  // SDA
#define LCD_CS   10  // CS
#define LCD_DC    9  // D/C
#define LCD_RST   8  // RST can use any pin
#define SCR_WD 160
#define SCR_HT 128
#define pgm_read_word(addr) (*(const unsigned short *)(addr))

#define PCD8544_CHAR_HEIGHT 8

ST7789_t3 lcd = ST7789_t3(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCLK, LCD_RST);

// Neopixel setup
#define PIXELPIN   7 
#define NUMPIXELS 24 
#define DELAYVAL 500 
int BRIGHTVAL = 5; 
// Adafruit_NeoPixel ring(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// DigiFont setup
#define RGBto565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define	GREY  RGBto565(128,128,128)
#define	LGREY RGBto565(160,160,160)
#define	DGREY RGBto565( 80, 80, 80)
#define	LBLUE RGBto565(100,100,255)
#define	DBLUE RGBto565(  0,  0,128)

void customLineH(int x0,int x1, int y, int c)    { lcd.drawFastHLine(x0,y,x1-x0+1,c); }
void customLineV(int x, int y0,int y1, int c)    { lcd.drawFastVLine(x,y0,y1-y0+1,c); } 
void customRect(int x, int y,int w,int h, int c) { lcd.fillRect(x,y,w,h,c); } 
DigiFont font(customLineH, customLineV, customRect);

class MyRenderer : public MenuComponentRenderer {
public:
    void render(Menu const& menu) const {
      Serial.print("\nCurrent menu name: ");
      Serial.println(menu.get_name());
      lcd.fillScreen(BLACK);
        for (int i = 0; i < menu.get_num_components(); ++i) {
            MenuComponent const* cp_m_comp = menu.get_menu_component(i);
            cp_m_comp->render(*this);
	    Serial.print(cp_m_comp->get_name());
	    lcd.setCursor(0, (i+1) * PCD8544_CHAR_HEIGHT);
	    lcd.print(cp_m_comp->get_name());
            if (cp_m_comp->is_current()) {
	      lcd.print(" X ");
	      Serial.print("<<< ");
	    }
            Serial.println("");
        }
    }

    // did testing on these, really not sure what they do. 
    void render_menu_item(MenuItem const& menu_item) const {
      // lcd.setCursor(120, 1 * PCD8544_CHAR_HEIGHT);
      // lcd.print("1");
    }

    void render_back_menu_item(BackMenuItem const& menu_item) const {
      // lcd.setCursor(120, 2 * PCD8544_CHAR_HEIGHT);
      // lcd.print("2");
    }

    void render_numeric_menu_item(NumericMenuItem const& menu_item) const {
      // lcd.setCursor(120, 3 * PCD8544_CHAR_HEIGHT);
      // lcd.print("3");
    }

    void render_menu(Menu const& menu) const {
      // lcd.setCursor(120, 4 * PCD8544_CHAR_HEIGHT);
      // lcd.print("4");
    }
};
MyRenderer my_renderer;

// Menu callback function

void on_item1_selected(MenuComponent* p_menu_component) {
    lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
    Serial.println("Item1 Selectd");
    lcd.print("Item1 Selectd");
    delay(1500); // so we can look the result on the LCD
}

void on_item2_selected(MenuComponent* p_menu_component) {
    lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
    lcd.print("Item2 Selectd");
    Serial.println("Item2 Selectd");
    delay(1500); // so we can look the result on the LCD
}

void on_item3_selected(MenuComponent* p_menu_component) {
    lcd.setCursor(0, 6 * PCD8544_CHAR_HEIGHT);
    lcd.print("Item3 Selectd");
    Serial.println("Item3 Selectd");
    delay(1500); // so we can look the result on the LCD
}


// Menu variables

MenuSystem ms(my_renderer);
MenuItem mm_mi1("Lvl1-Item1(I)", &on_item1_selected);
MenuItem mm_mi2("Lvl1-Item2(I)", &on_item2_selected);
Menu mu1("Lvl1-Item3(M)");
MenuItem mu1_mi1("Lvl2-Item1(I)", &on_item3_selected);

void serial_print_help() {
    Serial.println("***************");
    Serial.println("w: go to previus item (up)");
    Serial.println("s: go to next item (down)");
    Serial.println("a: go back (right)");
    Serial.println("d: select \"selected\" item");
    Serial.println("?: print this help");
    Serial.println("h: print this help");
    Serial.println("***************");
}

void serial_handler() {
    char inChar;
    if((inChar = Serial.read())>0) {
        switch (inChar) {
            case 'w': // Previus item
                ms.prev();
                ms.display();
                break;
            case 's': // Next item
                ms.next();
                ms.display();
                break;
            case 'a': // Back pressed
                ms.back();
                ms.display();
                break;
            case 'd': // Select pressed
                ms.select();
                ms.display();
                break;
            case '?':
            case 'h': // Display help
                serial_print_help();
                break;
            default:
                break;
        }
    }
}

// Standard arduino functions

void setup() {
    Serial.begin(9600);

    Serial.begin(9600);
    lcd.initR(INITR_BLACKTAB); // for 128x160 display
    lcd.setRotation(3);

    lcd.fillScreen(BLACK);

    lcd.setTextSize(1);

    serial_print_help();

    ms.get_root_menu().add_item(&mm_mi1);
    ms.get_root_menu().add_item(&mm_mi2);
    ms.get_root_menu().add_menu(&mu1);
    mu1.add_item(&mu1_mi1);
    ms.display();
}

void loop() {
    serial_handler();
}

