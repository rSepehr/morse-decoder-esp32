#include <Arduino.h>
#include "config.h"
#include "buzzer.h"
#include "display.h"
#include "morse.h"

static morse::Decoder decoder(INITIAL_UNIT_MS, LONG_PRESS_MS);

static int lastRawLevel = HIGH;
static int stableLevel = HIGH;
static uint32_t lastDebounceTime = 0;

static uint32_t lastRevision = 0xFFFFFFFF;
static bool lastPressedState = false;
static uint32_t lastRenderTime = 0;

void setup() {
    Serial.begin(115200);

    buzzerInit();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    displayInit();

#if SELF_TEST == 1
    displaySelfTest();
#else
    displaySplash();
#endif

    displayRender(decoder);
}

void loop() {
    uint32_t now = millis();

    // 2. Debounce push button
    int raw = digitalRead(BUTTON_PIN);
    if (raw != lastRawLevel) {
        lastDebounceTime = now;
        lastRawLevel = raw;
    }

    if ((now - lastDebounceTime) >= DEBOUNCE_MS) {
        if (raw != stableLevel) {
            stableLevel = raw;
            // 3. Accepted press edge (pressed = LOW with pull-up)
            if (stableLevel == LOW) {
                buzzerOn();
                decoder.onPress(now);
            }
            // 4. Accepted release edge (released = HIGH)
            else {
                buzzerOff();
                decoder.onRelease(now);
            }
        }
    }

    // 5. Poll decoder
    morse::Event e = decoder.poll(now);
    if (e == morse::Event::Clear) {
        buzzerConfirm();
        if (stableLevel == LOW) {
            buzzerOn();
        }
        Serial.print("[EVENT] Clear: ");
        Serial.println(decoder.text());
    } else if (e == morse::Event::Letter) {
        Serial.print("[EVENT] Letter: ");
        Serial.println(decoder.text());
    } else if (e == morse::Event::WordSpace) {
        Serial.print("[EVENT] WordSpace: ");
        Serial.println(decoder.text());
    } else if (e == morse::Event::Error) {
        Serial.print("[EVENT] Error: ");
        Serial.println(decoder.text());
    }

    // 6. Display rendering throttle
    uint32_t currentRev = decoder.revision();
    bool currentPressed = decoder.isPressed();
    if ((currentRev != lastRevision || currentPressed != lastPressedState) &&
        (now - lastRenderTime >= RENDER_INTERVAL_MS)) {
        lastRevision = currentRev;
        lastPressedState = currentPressed;
        lastRenderTime = now;
        displayRender(decoder);
    }
}
