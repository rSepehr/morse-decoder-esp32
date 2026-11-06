#include "buzzer.h"
#include "config.h"
#include <Arduino.h>

void buzzerInit() {
    pinMode(BUZZER_PIN, OUTPUT);
    buzzerOff();
}

void buzzerOn() {
#if BUZZER_MODE == BUZZER_ACTIVE_HIGH
    digitalWrite(BUZZER_PIN, HIGH);
#elif BUZZER_MODE == BUZZER_ACTIVE_LOW
    digitalWrite(BUZZER_PIN, LOW);
#elif BUZZER_MODE == BUZZER_PASSIVE
    tone(BUZZER_PIN, BUZZER_TONE_HZ);
#endif
}

void buzzerOff() {
#if BUZZER_MODE == BUZZER_ACTIVE_HIGH
    digitalWrite(BUZZER_PIN, LOW);
#elif BUZZER_MODE == BUZZER_ACTIVE_LOW
    digitalWrite(BUZZER_PIN, HIGH);
#elif BUZZER_MODE == BUZZER_PASSIVE
    noTone(BUZZER_PIN);
#endif
}

void buzzerConfirm() {
    buzzerOn();
    delay(60);
    buzzerOff();
    delay(60);
    buzzerOn();
    delay(60);
    buzzerOff();
}
