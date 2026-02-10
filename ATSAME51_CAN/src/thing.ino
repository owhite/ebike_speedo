#include <CAN.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <can_ids.h> // from: MESC_Firmware/MESC_RTOS/Tasks/can_ids.h
#include <Adafruit_LEDBackpack.h>
#include "debug.h"
#include <string.h>   // memcpy, strtok
#include <stdlib.h>   // atoi
#include <FlashStorage_SAMD.h>

Adafruit_AlphaNum4 alphaLED = Adafruit_AlphaNum4();

#define BTN_PIN1       12
#define BTN_PIN2       13
#define KILLSWITCH_PIN 17

#define CAN_PACKET_SIZE  8
#define CAN_DEBUG        0

/////////
// bike constants
static const float CIRCUMFERENCE_CM = 219.5f;   // measured with tape
static const float CM_IN_MILE       = 160900.0f;

#define MINVOLTAGE 58
#define MAXVOLTAGE 75
#define MAXTEMP    70

/////////
// Persistent config (Flash)
static const uint16_t CONFIG_MAGIC = 0xB17E;
static const int DEFAULT_POLEPAIRS = 7;

// Your sprockets (given)
static const int DEFAULT_FRONT_SPROCKET = 14;
static const int DEFAULT_BACK_SPROCKET  = 98;

struct PersistConfig {
  uint16_t magic;
  int32_t polepairs;
  int32_t front_sprocket;
  int32_t back_sprocket;
};

FlashStorage(config_storage, PersistConfig);
PersistConfig cfg;

// Runtime (used by math / display)
int   POLEPAIRS_i        = DEFAULT_POLEPAIRS;
int   FRONT_SPROCKET_i   = DEFAULT_FRONT_SPROCKET;
int   BACK_SPROCKET_i    = DEFAULT_BACK_SPROCKET;

float sprocket_ratio_f   = 1.0f;  // Tf/Tb
float mph_per_ehz_scale  = 0.0f;  // multiplier to convert EHz -> mph

static void recalcGearRatioAndScale();
static void loadConfig();
static void saveConfig();

/////////
// CAN variables
unsigned long canReceiveTime = 0;
const unsigned long canInterval = 2000;

uint8_t  canBuf[8] = {};
uint16_t packetId = 0;

float canData1_f = 0.0f;
float canData2_f = 0.0f;

/////////
// reporting things
unsigned long currentTime = 0;
unsigned long reportLastTime = 0;
const unsigned long reportInterval = 800;

const uint8_t prefixes[] = {'M', 'E', 'B', 'A', 'T'};

enum ReportIndex : uint8_t {
  IDX_MPH  = 0,
  IDX_EHZ  = 1,
  IDX_VBUS = 2,
  IDX_IBUS = 3,
  IDX_TEMP = 4,
  IDX_COUNT
};

const uint8_t userRequestMax = IDX_COUNT;

float reportedValue = 0.0f;
float reportValues[IDX_COUNT] = {0};

uint8_t userRequest   = 0; // what the user wants
uint8_t reportInc     = 0; // cycles through things to display
uint8_t lastReportInc = 0;

enum FlagIndex : uint8_t {
  FLAG_CAN_TIMEOUT = 0,
  FLAG_ERROR       = 1,
  FLAG_TEMP        = 2,
  FLAG_COUNT
};

bool flagList[FLAG_COUNT] = {false, false, false}; // can, error, temp

/////////
// button reading timers
const int SHORT_PRESS_TIME = 600;
const int LONG_PRESS_TIME  = 600;

struct ButtonState {
  uint8_t pin;
  int lastState;
  int currentState;
  unsigned long pressedTime;
  unsigned long releasedTime;
};

ButtonState btn1 = { BTN_PIN1, LOW, LOW, 0, 0 };
ButtonState btn2 = { BTN_PIN2, LOW, LOW, 0, 0 };

// control if LED is bright or dim
bool brightnessToggle = true;

static inline void setBrightnessFromToggle() {
  alphaLED.setBrightness(brightnessToggle ? 14 : 0);
}

static inline void clampUserRequest() {
  if (userRequest >= userRequestMax) userRequest = 0;
}

void displayNum3(int n);
void zipDisplay(int c);
uint16_t extract_id(uint32_t ext_id);

void canStatus();
void errorStatus();
void tempStatus();

void handleButton(ButtonState &b);
void onShortPress();
void onLongPress();

///////
// Serial command handling (no Arduino String)
static const uint16_t SERIAL_LINE_MAX = 64;
char serialLine[SERIAL_LINE_MAX];
uint16_t serialLen = 0;

static void pollSerial();
static void handleSerialLine(char *line);
static void printHelp();
static void printConfig();

void setup() {
  Serial.begin(9600);

  // Load persistent values and recalc on boot
  loadConfig();
  recalcGearRatioAndScale();

  pinMode(btn1.pin, INPUT);
  pinMode(btn2.pin, INPUT);

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false); // standby off
  digitalWrite(PIN_CAN_BOOSTEN, true);  // booster on

  if (!CAN.begin(500000)) {
    DEBUG_PRINTLN("Starting CAN failed!");
    while (1) {}
  }

  CAN.onReceive(onReceive); // register CAN callback

  alphaLED.begin(0x70);
  setBrightnessFromToggle();

  alphaLED.clear();
  alphaLED.writeDigitAscii(1, 'O');
  alphaLED.writeDigitAscii(2, 'F');
  alphaLED.writeDigitAscii(3, 'F');
  alphaLED.writeDisplay();

  btn1.pressedTime = millis();
  btn2.pressedTime = millis();

  zipDisplay(0xBFFF);
  zipDisplay(0xBFFF);
  zipDisplay(0xBFFF);

  // Quick hint at boot
  Serial.println();
  Serial.println("Ready. Type 'H' for help.");
  printConfig();
}

void loop() {
  pollSerial();

  canStatus();
  errorStatus();
  tempStatus();

  // If you want both buttons active, call both:
  // handleButton(btn1);
  handleButton(btn2);

  currentTime = millis();

  // periodic check to change display based on errors
  if ((currentTime - reportLastTime) > reportInterval) {
    lastReportInc = reportInc;

    // reportInc meaning:
    // 0: user display
    // 1: CAN timeout
    // 2: error
    // 3: temp
    int i;
    for (i = (int)lastReportInc + 1; i < 4; i++) {
      if (flagList[i - 1]) { // (1..3) maps to (0..2)
        reportInc = (uint8_t)i;
        break;
      }
    }
    if (i == 4) { reportInc = 0; }

    reportLastTime = currentTime;
  }

  switch (reportInc) {
    case 0: // show what the user wants
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, prefixes[userRequest]);
      displayNum3((int)reportValues[userRequest]); // show on digits 1..3
      alphaLED.writeDisplay();
      reportedValue = reportValues[userRequest];
      break;

    case 1: // show CAN has timed out
      alphaLED.clear();
      alphaLED.writeDigitAscii(1, 'C');
      alphaLED.writeDigitAscii(2, 'A');
      alphaLED.writeDigitAscii(3, 'N');
      alphaLED.writeDisplay();
      break;

    case 2: // show error status
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, 'E');
      displayNum3((int)reportValues[userRequest]); // placeholder until error value is real
      alphaLED.writeDisplay();
      break;

    case 3: // show temp status
      alphaLED.clear();
      alphaLED.writeDigitAscii(0, 'T');
      alphaLED.writeDigitAscii(1, 'E');
      alphaLED.writeDigitAscii(2, 'M');
      alphaLED.writeDigitAscii(3, 'P');
      alphaLED.writeDisplay();
      break;

    default:
      break;
  }
}

void canStatus() {
  flagList[FLAG_CAN_TIMEOUT] = false;
  currentTime = millis();
  if ((currentTime - canReceiveTime) > canInterval) {
    flagList[FLAG_CAN_TIMEOUT] = true;
  }
}

void errorStatus() {
  flagList[FLAG_ERROR] = false;
}

void tempStatus() {
  flagList[FLAG_TEMP] = false;
}

void onReceive(int packetSize) {
  packetId = extract_id(CAN.packetId());

  if (packetSize != CAN_PACKET_SIZE) return;

  for (int i = 0; i < packetSize; i++) {
    canBuf[i] = (uint8_t)CAN.read();
  }

  // Same endianness assumption as your original union+byte assignment
  memcpy(&canData1_f, &canBuf[0], 4);
  memcpy(&canData2_f, &canBuf[4], 4);

  switch (packetId) {
    case CAN_ID_SPEED:
      // canData1_f is EHz
      reportValues[IDX_EHZ] = canData1_f;

      // mph = EHz * ( (C_cm * 3600 / CM_PER_MILE) * (Tf/Tb) / polepairs )
      reportValues[IDX_MPH] = reportValues[IDX_EHZ] * mph_per_ehz_scale;

      canReceiveTime = millis();
      break;

    case CAN_ID_BUS_VOLT_CURR:
      reportValues[IDX_VBUS] = canData1_f;
      reportValues[IDX_IBUS] = canData2_f;
      canReceiveTime = millis();
      break;

    case CAN_ID_TEMP_MOT_MOS1:
      reportValues[IDX_TEMP] = canData2_f;
      canReceiveTime = millis();
      break;

    default:
      break;
  }
}

// Writes an integer into digits 1..3 (keeps digit 0 free for a prefix).
// Right-justified. Handles negatives. If out of range, shows "OVF".
void displayNum3(int n) {
  // Clear digits 1..3
  alphaLED.writeDigitAscii(1, ' ');
  alphaLED.writeDigitAscii(2, ' ');
  alphaLED.writeDigitAscii(3, ' ');

  // Range: -99..999 fits naturally into 3 characters with optional sign.
  if (n > 999 || n < -99) {
    alphaLED.writeDigitAscii(1, 'O');
    alphaLED.writeDigitAscii(2, 'V');
    alphaLED.writeDigitAscii(3, 'F');
    return;
  }

  char buf[4] = {' ', ' ', ' ', '\0'}; // 3 chars + null

  bool neg = (n < 0);
  int v = neg ? -n : n;

  // Build from right to left
  int pos = 2;
  if (v == 0) {
    buf[pos--] = '0';
  } else {
    while (v > 0 && pos >= 0) {
      buf[pos--] = char('0' + (v % 10));
      v /= 10;
    }
  }

  if (neg) {
    if (pos >= 0) {
      buf[pos] = '-';
    } else {
      buf[0] = '-';
    }
  }

  alphaLED.writeDigitAscii(1, buf[0]);
  alphaLED.writeDigitAscii(2, buf[1]);
  alphaLED.writeDigitAscii(3, buf[2]);
}

void onShortPress() {
  userRequest++;
  clampUserRequest();
  DEBUG_PRINTLN("short press");
  delay(10);
  reportInc = 0;
}

void onLongPress() {
  DEBUG_PRINTLN("long press");
  brightnessToggle = !brightnessToggle;
  setBrightnessFromToggle();
  delay(10);
  reportInc = 0;
}

void handleButton(ButtonState &b) {
  b.currentState = digitalRead(b.pin);

  if (b.lastState == HIGH && b.currentState == LOW) { // press
    b.pressedTime = millis();
  }
  else if (b.lastState == LOW && b.currentState == HIGH) { // release
    b.releasedTime = millis();
    unsigned long dt = b.releasedTime - b.pressedTime;

    if ((int)dt < SHORT_PRESS_TIME) onShortPress();
    if ((int)dt > LONG_PRESS_TIME)  onLongPress();
  }

  b.lastState = b.currentState;
}

// the delays make this blocking (left as-is)
void zipDisplay(int c) {
  alphaLED.writeDigitRaw(3, 0x0);
  alphaLED.writeDigitRaw(0, c);
  alphaLED.writeDisplay();
  delay(100);

  alphaLED.writeDigitRaw(0, 0x0);
  alphaLED.writeDigitRaw(1, c);
  alphaLED.writeDisplay();
  delay(100);

  alphaLED.writeDigitRaw(1, 0x0);
  alphaLED.writeDigitRaw(2, c);
  alphaLED.writeDisplay();
  delay(100);

  alphaLED.writeDigitRaw(2, 0x0);
  alphaLED.writeDigitRaw(3, c);
  alphaLED.writeDisplay();
  delay(100);

  alphaLED.clear();
  alphaLED.writeDisplay();
}

uint16_t extract_id(uint32_t ext_id) {
  return (uint16_t)(ext_id >> 16);
}

/////////////////////////////
// Flash + config handling  //
/////////////////////////////

void loadConfig() {
  PersistConfig tmp;
  config_storage.read(tmp);   // read into object

  if (tmp.magic != CONFIG_MAGIC) {
    tmp.magic = CONFIG_MAGIC;
    tmp.polepairs = DEFAULT_POLEPAIRS;
    tmp.front_sprocket = DEFAULT_FRONT_SPROCKET; // 14
    tmp.back_sprocket  = DEFAULT_BACK_SPROCKET;  // 98
    config_storage.write(tmp);
  }

  cfg = tmp;
  POLEPAIRS_i      = (int)cfg.polepairs;
  FRONT_SPROCKET_i = (int)cfg.front_sprocket;
  BACK_SPROCKET_i  = (int)cfg.back_sprocket;
}

void saveConfig() {
  cfg.magic = CONFIG_MAGIC;
  cfg.polepairs = POLEPAIRS_i;
  cfg.front_sprocket = FRONT_SPROCKET_i;
  cfg.back_sprocket  = BACK_SPROCKET_i;

  config_storage.write(cfg);
}

static void recalcGearRatioAndScale() {
  // sprocket_ratio = Tf / Tb
  if (FRONT_SPROCKET_i > 0 && BACK_SPROCKET_i > 0) {
    sprocket_ratio_f = (float)FRONT_SPROCKET_i / (float)BACK_SPROCKET_i;
  } else {
    sprocket_ratio_f = 1.0f;
  }

  // mph_per_ehz_scale = (C_cm * 3600 / CM_PER_MILE) * (Tf/Tb) / polepairs
  if (POLEPAIRS_i <= 0) POLEPAIRS_i = DEFAULT_POLEPAIRS;

  mph_per_ehz_scale =
      (CIRCUMFERENCE_CM * 3600.0f / CM_IN_MILE) *
      sprocket_ratio_f /
      (float)POLEPAIRS_i;

  // Keep compatibility with your previous variable name (optional)
  // (so any prints you already expect still work)
  // "EHZ_scale" previously meant "mph per EHz"
  // If you don't want this alias, you can remove EHZ_scale entirely.
  // (Left here because your printConfig() expects it.)
  // NOTE: declared above as mph_per_ehz_scale now.
}

/////////////////////////////
// Serial command handling  //
/////////////////////////////

static void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    Serial.print(c);

    // Treat CR or LF as end-of-line
    if (c == '\n' || c == '\r') {
      if (serialLen > 0) {
        serialLine[serialLen] = '\0';
        handleSerialLine(serialLine);
        serialLen = 0;
      }
      continue;
    }

    // Basic line buffer (drop extra chars if too long)
    if (serialLen < (SERIAL_LINE_MAX - 1)) {
      serialLine[serialLen++] = c;
    }
  }
}

static void handleSerialLine(char *line) {
  // Trim leading spaces
  while (*line == ' ' || *line == '\t') line++;

  if (*line == '\0') return;

  // Tokenize: CMD [value]
  char *cmdTok = strtok(line, " \t");
  if (!cmdTok) return;

  char cmd = cmdTok[0];
  if (cmd >= 'a' && cmd <= 'z') cmd = (char)(cmd - 32); // to upper

  char *valTok = strtok(NULL, " \t");

  Serial.println();

  switch (cmd) {
    case 'H':
      printHelp();
      break;

    case 'S':
      // Save + recalc
      saveConfig();
      recalcGearRatioAndScale();
      Serial.println("Saved. Recalculated mph scale from sprockets + polepairs.");
      printConfig();
      break;

    case 'P': {
      if (!valTok) { Serial.println("Usage: P <int>"); break; }
      int v = atoi(valTok);
      if (v <= 0 || v > 100) { Serial.println("Polepairs must be >0 (and reasonable)."); break; }
      POLEPAIRS_i = v;
      recalcGearRatioAndScale(); // update immediately (not persisted until 'S')
      Serial.print("Set polepairs = "); Serial.println(POLEPAIRS_i);
      Serial.println("Note: not saved until you type 'S'.");
      break;
    }

    case 'F': {
      if (!valTok) { Serial.println("Usage: F <int>"); break; }
      int v = atoi(valTok);
      if (v <= 0 || v > 200) { Serial.println("front_sprocket must be >0 (and reasonable)."); break; }
      FRONT_SPROCKET_i = v;
      recalcGearRatioAndScale();
      Serial.print("Set front_sprocket = "); Serial.println(FRONT_SPROCKET_i);
      Serial.println("Note: not saved until you type 'S'.");
      break;
    }

    case 'B': {
      if (!valTok) { Serial.println("Usage: B <int>"); break; }
      int v = atoi(valTok);
      if (v <= 0 || v > 500) { Serial.println("back_sprocket must be >0 (and reasonable)."); break; }
      BACK_SPROCKET_i = v;
      recalcGearRatioAndScale();
      Serial.print("Set back_sprocket = "); Serial.println(BACK_SPROCKET_i);
      Serial.println("Note: not saved until you type 'S'.");
      break;
    }

    default:
      Serial.print("Unknown command: ");
      Serial.println(cmdTok);
      Serial.println("Type 'H' for help.");
      break;
  }
}

static void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  H              - help");
  Serial.println("  S              - save to flash and recalc mph scale");
  Serial.println("  P <int>        - set polepairs");
  Serial.println("  F <int>        - set front_sprocket teeth");
  Serial.println("  B <int>        - set back_sprocket teeth");
  Serial.println();
  printConfig();
}

static void printConfig() {
  Serial.print("polepairs        = "); Serial.println(POLEPAIRS_i);
  Serial.print("front_sprocket   = "); Serial.println(FRONT_SPROCKET_i);
  Serial.print("back_sprocket    = "); Serial.println(BACK_SPROCKET_i);
  Serial.print("sprocket_ratio   = "); Serial.println(sprocket_ratio_f, 8);    // Tf/Tb
  Serial.print("mph_per_ehz_scale= "); Serial.println(mph_per_ehz_scale, 10);  // mph per EHz
  Serial.println();
}
