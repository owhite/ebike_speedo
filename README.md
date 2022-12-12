# EBIKE SPEEDOMETER PCB FEATURES
* Super boring 14 segment LED displays
* Adafruit HUZZAH32 ESP32 Feather
* Serial data --> board --> BLE to smartphone
* Compatible with [MESC controller](https://github.com/davidmolony/MESC_Firmware)

## Operation
Device connects to an ESC (e.g. the [MP2](https://github.com/badgineer/CCC_ESC) and receives data via a serial output. Data is in the form of json. Typcial data stream could include:
```
{"amps": 10, "volts": 20, "rpm": 200, "temp": 90}
```
The speeddometer is expecting certain tags and ignores others. For amps, volts, rpm and temp, these values as ints will be displayed on the 14 segment LED. RPM will be converted to MPH prior to display. 

Regardless of the values or data structure received, the entired data stream is passed on to a bluetooth device which can be captured on your smartphone using an app like [Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal). 

## Programming
* use the [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) to progam the microcontroller on the ESC
* use [platformio](https://platformio.org/) for the ESP32. 

## ESP32 quirks
There are conflicts between bootstrapping capability and usage of some pins. If you notice your board wont program unless you keep the button depressed, it might be due to this conflict.

This reqiures that you burn the flash voltage selection eFuses change the internal regulator’s output voltage to 3.3 V. This was fixed with: 

```
$ espefuse.py set_flash_voltage 3.3V --port /dev/cu.usbserial-01562A86
```

<img src="pics/3D_render.png" title="3D speedometer">
