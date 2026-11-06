#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// GPIO connected to the momentary Morse push button (active-low with internal pull-up)
static constexpr uint8_t BUTTON_PIN = 27;

// GPIO connected to the buzzer I/O signal pin
static constexpr uint8_t BUZZER_PIN = 26;

// Buzzer mode identifiers
#define BUZZER_ACTIVE_HIGH 0
#define BUZZER_ACTIVE_LOW  1
#define BUZZER_PASSIVE     2

// Active buzzer mode selection (configured as BUZZER_ACTIVE_LOW)
#ifndef BUZZER_MODE
#define BUZZER_MODE BUZZER_ACTIVE_LOW
#endif

// Tone frequency in Hertz for passive buzzer operation
static constexpr uint32_t BUZZER_TONE_HZ = 2700;

// Holding button for this duration (milliseconds) clears history and text
static constexpr uint32_t LONG_PRESS_MS = 2000;

// Starting estimate for dot duration in milliseconds (~8 WPM initial baseline)
static constexpr uint32_t INITIAL_UNIT_MS = 150;

// Button debounce stability time in milliseconds
static constexpr uint32_t DEBOUNCE_MS = 15;

// Minimum interval in milliseconds between TFT display screen updates
static constexpr uint32_t RENDER_INTERVAL_MS = 40;

// Set to 1 to run display color bars and buzzer test at system boot
#define SELF_TEST 0

// TFT_eSPI rotation. 3 = landscape, 90 degrees counterclockwise from portrait. If the image appears upside down, use 1.
static constexpr uint8_t DISPLAY_ROTATION = 3;

#endif // CONFIG_H
