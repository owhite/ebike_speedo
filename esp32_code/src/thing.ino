/*
  STM32F405 code snippet
  static unsigned short pin_state = 0;
  while (1) {
		HAL_Delay(100);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, pin_state);
		pin_state = !pin_state;

		char transmit_buffer[100];
		int sizebuff;
		sizebuff = sprintf(transmit_buffer,"{\"amps\":%d, \"volts\":%d}\r\n", rand() %  200, rand() % 200);
		HAL_UART_Transmit_DMA(&huart3, transmit_buffer, sizebuff);

  }
*/ 

#include <ArduinoJson.h>

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

char inChar;
char oldChar;
char inStr[100] = "";
int count = 0;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  SerialBT.begin("ESP32test"); //Bluetooth device name

  Serial.println("The device started, now you can pair it with bluetooth!");
  pinMode(13, OUTPUT);

}


void loop() {
  delay(100);
  Serial.println("beep");

  if (Serial1.available()) {
    inChar = Serial1.read();
    if (inChar == '\n' && oldChar == '\r') {
      inStr[count - 1] = '\0';
      // Serial.println(inStr);
      SerialBT.println(inStr);

      StaticJsonDocument<200> doc;

      DeserializationError error = deserializeJson(doc, inStr);
      if (error) {
	Serial.print(F("deserializeJson() failed: "));
	Serial.println(error.f_str());
      }
      else {
	uint16_t amps = doc["amps"];
	uint16_t volts = doc["volts"];
	// Serial.println(amps);
	// Serial.println(volts);
      }

      count = 0;
    }
    else {
      inStr[count] = inChar;
      count++;
    }
    // Serial.write(inChar);
    if (inChar != '\n') {
      // SerialBT.write(inChar);
    }
    oldChar = inChar;
  }
}
