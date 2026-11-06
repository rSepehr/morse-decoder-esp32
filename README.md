# ESP32 Morse Code Decoder

A production-quality, open-source embedded firmware for ESP32 that decodes Morse code keyed with a single push button and renders both keyed symbols and decoded English text on an ILI9341 TFT display.

## Features

- Real-time Morse keying via a single push button with audible buzzer sidetone.
- Pure C++17 adaptive speed estimation (WPM) using two-group clustering and dynamic gap learning.
- ITU-compliant Morse decoding covering letters A-Z, digits 0-9, and standard punctuation marks.
- 2.8" SPI TFT display in landscape (320x240) showing live keying status, Morse symbol history, and word-wrapped English text.
- Flicker-free differential rendering with built-in fonts and zero dynamic memory allocations in the main execution loop.
- Long-press clear gesture (hold 2 seconds) with audible double-beep confirmation.
- Comprehensive native test suite covering timing classification, jitter tolerance, speed adaptation, Farnsworth spacing, and edge cases.

## Hardware List

- ESP32-WROOM-32 DevKit (38-pin module).
- 2.8" SPI TFT LCD module, 240x320 native resolution, ILI9341 controller (operated in landscape 320x240, touch pins disconnected).
- 3-pin Buzzer module (supporting active-high, active-low, or passive piezoceramic modes).
- Momentary push button.
- Breadboard and jumper wires.

## Wiring Summary Table

| Component       | Pin Label       | ESP32 Pin / Rail  | Description                          |
|-----------------|-----------------|-------------------|--------------------------------------|
| TFT Display     | VCC             | 3V3               | 3.3V Power rail                      |
| TFT Display     | GND             | GND               | Ground rail                          |
| TFT Display     | CS              | GPIO 5            | SPI Chip Select                      |
| TFT Display     | RESET           | GPIO 17           | Hardware Reset                       |
| TFT Display     | DC / RS         | GPIO 16           | Data / Command Selection             |
| TFT Display     | SDI (MOSI)      | GPIO 23           | VSPI MOSI                            |
| TFT Display     | SCK (CLK)       | GPIO 18           | VSPI Clock                           |
| TFT Display     | LED             | GPIO 32           | Backlight enable (active HIGH)       |
| TFT Display     | SDO (MISO)      | GPIO 19           | VSPI MISO                            |
| Buzzer Module   | VCC             | 3V3               | Power rail                           |
| Buzzer Module   | GND             | GND               | Ground rail                          |
| Buzzer Module   | I/O             | GPIO 26           | Buzzer control signal                |
| Push Button     | Leg 1           | GPIO 27           | Key input (internal pull-up enabled) |
| Push Button     | Leg 2           | GND               | Ground return                        |

For a complete ASCII schematic and design notes, see `docs/wiring.md`.

## Build and Flash Commands

Run the native test suite on your development host:

```bash
pio test -e native
```

Compile and flash the firmware to the ESP32:

```bash
pio run -e esp32dev -t upload
```

To monitor Serial output (115200 baud):

```bash
pio device monitor -b 115200
```

## Usage

- Keying Dots and Dashes: Press the push button momentarily. The buzzer emits sidetone while the button is pressed down. A press shorter than the adaptive threshold is classified as a dot (.), and a longer press is classified as a dash (-).
- Letter Gaps: Pause keying for at least the letter boundary L. The decoder commits the completed letter, looks up its character, appends it to the decoded text, and records the symbols in the Morse history.
- Word Gaps: Pause keying for at least the word boundary W. The decoder inserts a word boundary space in the decoded text and appends " /" to the Morse history.
- Clearing History: Hold the push button continuously for at least 2000 ms (2 seconds). The decoder clears the decoded text, Morse history, and symbol buffers, and plays a double confirmation beep. Learned speed and gap models are retained.

## Configuration Table

All hardware pin assignments and operational parameters are defined in `include/config.h`:

| Constant             | Default Value        | Description                                                                                             |
|----------------------|----------------------|---------------------------------------------------------------------------------------------------------|
| `BUTTON_PIN`         | `27`                 | GPIO pin connected to the Morse key button.                                                             |
| `BUZZER_PIN`         | `26`                 | GPIO pin connected to the buzzer I/O control pin.                                                      |
| `BUZZER_MODE`        | `BUZZER_ACTIVE_LOW`  | Buzzer driver: `0` (Active High), `1` (Active Low), `2` (Passive).                                      |
| `BUZZER_TONE_HZ`     | `2700`               | Audio tone frequency in Hz used when `BUZZER_MODE` is passive.                                          |
| `LONG_PRESS_MS`      | `2000`               | Hold duration in milliseconds required to clear history.                                                |
| `INITIAL_UNIT_MS`    | `150`                | Initial dot unit length assumption (~8 WPM baseline).                                                   |
| `DEBOUNCE_MS`        | `15`                 | Push button contact debounce settling time in milliseconds.                                             |
| `RENDER_INTERVAL_MS` | `40`                 | Minimum throttle period in milliseconds between TFT display redraws.                                    |
| `SELF_TEST`          | `0`                  | Set to `1` to run color bars and buzzer verification at power-on.                                       |
| `DISPLAY_ROTATION`   | `3`                  | TFT_eSPI rotation: `3` = landscape (90 deg counterclockwise). If the image is upside down, set to `1`.  |

Display SPI pins and controller driver flags are configured in `platformio.ini` under `build_flags`.

## Troubleshooting

- Wrong colors or swapped red/blue: Check the display controller driver or add `-DTFT_RGB_ORDER=TFT_BGR` (or `TFT_RGB`) in `platformio.ini` build flags.
- Blank display: Verify the TFT LED backlight connection on GPIO 32, ensure 3V3 power is stable, and inspect the solder bridges on the display module.
- Image upside down: Set `DISPLAY_ROTATION` to `1` in `include/config.h`.
- Letters split too early: Send one warm-up sentence first (such as "PARIS PARIS "), then key slowly.
- Buzzer silent or always on: Ensure `BUZZER_MODE` in `include/config.h` matches your physical module. Use `BUZZER_ACTIVE_HIGH` for standard active buzzers, `BUZZER_ACTIVE_LOW` for inverted active modules, or `BUZZER_PASSIVE` for passive piezoceramic transducers.
- First characters wrong: The decoder features an adaptive speed tracker. When starting from an unknown keying speed, sending "PARIS" once at the start locks in cadence.

## Design Decisions

- ITU-R M.1677 timing ratios: Standard Morse timing proportions (dot = 1 unit, dash = 3 units, intra-symbol gap = 1 unit, letter gap = 3 units, word gap = 7 units) form the baseline model.
- Dot/dash boundary at the midpoint of two units, using learned dot and dash clusters: Press durations are clustered via an optimal two-group variance split, yielding an adaptive unit estimate U and threshold T = 2.0 * U.
- Intra-letter, letter and word gaps learned separately: The decoder maintains dedicated running estimators for intra-element gaps, inter-letter gaps, and inter-word gaps using incoming gap observations.
- The letter boundary never drops below 2.5 units: Letter boundary L is clamped to [2.5 * U, 4.0 * U], preventing slow intra-character pauses from prematurely committing letters.
- Farnsworth spacing supported after warm-up: Operators using elongated inter-letter and inter-word gaps are accommodated because word boundary W adapts upward dynamically (up to 12 * U).
- Polling instead of ISR: Button debouncing and decoder evaluation are processed inside a non-blocking `loop()` cycle rather than hardware interrupts (ISRs). This prevents race conditions, eliminates concurrency overhead, and simplifies maintenance.
- English-only output: High-efficiency built-in ASCII bitmap fonts (Font 2 and Font 4) from TFT_eSPI are used. Persian rendering and complex bidirectional shaping are excluded to guarantee deterministic sub-millisecond redraw latency and zero dynamic allocation.
- The 2 s clear gesture: Holding the key for 2000 ms provides an intuitive physical reset without requiring secondary buttons, accompanied by an audible two-tone beep.

## Tests

The project includes unit test suites executing under PlatformIO's native environment via the Unity test framework (`test/test_morse.cpp`).

Run tests locally:

```bash
pio test -e native
```

Test coverage includes:
1. Fresh decoder classification (100 ms dot vs. 400 ms dash).
2. Single letter commit ("E").
3. Multi-speed decoding of "SOS HELP" at 5, 10, 15, 20, and 25 WPM with simulated operator timing jitter (+/- 15%).
4. Multi-speed decoding of "HELLO WORLD 73" at 5-25 WPM with jitter.
5. Speed estimation accuracy at 12 WPM.
6. Malformed and overflowing symbol sequence rejection (outputs '?').
7. Exact Morse history formatting for multi-word sequences ("HI THERE").
8. Long-press clear behavior and release suppression.
9. Glitch rejection for noise pulses below 25 ms.
10. Slow letter "I" on a fresh decoder with 330 ms intra gap.
11. Slow intra gaps at 12 WPM with 1.6U spacing.
12. Farnsworth spacing with letter-gap 6U and word-gap 14U.

## Known Limitations

- The first characters are less reliable while the decoder learns.
- Sending "PARIS" once at the start gives the best results.
- Before learning, stretched letter gaps may be read as word spaces.
- No send mode: The firmware functions purely as a keying decoder; automated Morse beaconing or transmitter keying is not implemented.
- No WiFi / Networking: Network peripherals and wireless stacks are intentionally omitted to maximize battery efficiency and real-time execution simplicity.

## Contributing

Contributions are welcome. Please ensure that all native tests pass (`pio test -e native`) and code builds cleanly without warnings before submitting pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
