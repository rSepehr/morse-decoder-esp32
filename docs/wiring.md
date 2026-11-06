# Hardware Wiring Guide

This document describes the complete wiring for the ESP32 Morse Code Decoder, connecting the ESP32-WROOM-32 DevKit to the 2.8" SPI TFT display (ILI9341), buzzer module, and Morse push button.

## Pin Connection Summary

| Component       | Pin Label       | ESP32 GPIO / Rail | Notes                                |
|-----------------|-----------------|-------------------|--------------------------------------|
| TFT Display     | VCC             | 3V3               | 3.3V Power rail                      |
| TFT Display     | GND             | GND               | Ground rail                          |
| TFT Display     | CS              | GPIO 5            | SPI Chip Select                      |
| TFT Display     | RESET           | GPIO 17           | Hardware Reset                       |
| TFT Display     | DC / RS         | GPIO 16           | Data / Command Selection             |
| TFT Display     | SDI (MOSI)      | GPIO 23           | VSPI MOSI (Hardware SPI)             |
| TFT Display     | SCK (CLK)       | GPIO 18           | VSPI SCK (Hardware SPI)              |
| TFT Display     | LED             | GPIO 32           | Backlight control (active HIGH)      |
| TFT Display     | SDO (MISO)      | GPIO 19           | VSPI MISO (Hardware SPI)             |
| TFT Display     | T_CLK, T_CS,... | Not connected     | Touch interface is unused            |
| Buzzer Module   | VCC             | 3V3               | Power rail                           |
| Buzzer Module   | GND             | GND               | Ground rail                          |
| Buzzer Module   | I/O             | GPIO 26           | Sidetone output                      |
| Push Button     | Leg 1           | GPIO 27           | Key input (internal pull-up enabled) |
| Push Button     | Leg 2           | GND               | Key ground return (Pressed = LOW)    |

## ASCII Schematic Diagram

```
                 +-------------------+
                 |  ESP32-WROOM-32   |
                 |      DevKit       |
                 |                   |
                 |               3V3 |------+--------+
                 |               GND |---+  |        |
                 |                   |   |  |        |
                 |            GPIO 5 |---|--+--[CS]  |
                 |           GPIO 16 |---|--+--[DC]  |
                 |           GPIO 17 |---|--+--[RST] |
                 |           GPIO 18 |--(SCK)        |
                 |           GPIO 19 |--(MISO)       |
                 |           GPIO 23 |--(MOSI)       |
                 |           GPIO 32 |--(LED)        |
                 |                   |   |  |        |
                 |           GPIO 26 |---+  |        |  +-----------------+
                 |                   |   |  |        +--| VCC             |
                 |           GPIO 27 |---+  +-----------| GND  2.8" SPI   |
                 +-------------------+   |  |           |      ILI9341    |
                     |             |     |  |           |      TFT 240x320|
                     |             |     |  |           +-----------------+
                     |             |     |  |
                     |             |     |  +-----------+
                     |             |     +--------------| GND             |
                     |             |                    | I/O  Buzzer     |
                     |             +--------------------| VCC  Module     |
                     |                                  +-----------------+
                     |       Momentary
                     |      Push Button
                     +---------[ / ]---------+
                                             |
                                            GND
```

## Important Hardware Notes

1. Strapping Pins: GPIO 0, 2, 12, and 15 are avoided to ensure reliable booting and flashing.
2. Input-Only Pins: GPIO 34-39 lack internal pull-ups and output drivers, so standard general-purpose I/O pins are used.
3. Push Button: GPIO 27 is configured with internal pull-up (`INPUT_PULLUP`). When pressed, it connects directly to GND.
4. Backlight: GPIO 32 is driven HIGH by TFT_eSPI to illuminate the backlight.
5. Touch Controller: The 5 touch pins on the ILI9341 board (T_CLK, T_CS, T_DIN, T_DO, T_IRQ) are left floating/disconnected.
