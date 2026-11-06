#include "display.h"
#include "buzzer.h"
#include "config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstring>
#include <cstdio>

static TFT_eSPI tft = TFT_eSPI();

static uint32_t s_lastWpm = 0xFFFFFFFF;
static bool s_lastPressed = false;
static char s_lastSymbols[8] = {'\1', '\0'};
static char s_lastMorse[258] = {'\1', '\0'};
static char s_lastText[258] = {'\1', '\0'};
static bool s_layoutReady = false;

static void wrapText(const char* text, int maxPixelWidth, int maxVisibleLines,
                     char outputLines[][64], int& lineCount) {
    lineCount = 0;
    if (!text || text[0] == '\0') {
        return;
    }

    char lines[32][64];
    int count = 0;
    char currentLine[64];
    currentLine[0] = '\0';
    size_t currentLen = 0;

    size_t idx = 0;
    size_t textLen = std::strlen(text);

    while (idx < textLen && count < 32) {
        if (text[idx] == ' ') {
            if (currentLen > 0 && currentLen < sizeof(currentLine) - 1) {
                char candidate[64];
                std::memcpy(candidate, currentLine, currentLen);
                candidate[currentLen] = ' ';
                candidate[currentLen + 1] = '\0';
                if (tft.textWidth(candidate, 2) <= maxPixelWidth) {
                    currentLine[currentLen++] = ' ';
                    currentLine[currentLen] = '\0';
                }
            }
            idx++;
        } else {
            size_t wordStart = idx;
            while (idx < textLen && text[idx] != ' ') {
                idx++;
            }
            size_t wordLen = idx - wordStart;

            char candidate[64];
            size_t candLen = currentLen;
            if (currentLen > 0) {
                std::memcpy(candidate, currentLine, currentLen);
            }

            if (candLen + wordLen < sizeof(candidate)) {
                std::memcpy(candidate + candLen, text + wordStart, wordLen);
                candidate[candLen + wordLen] = '\0';
                if (tft.textWidth(candidate, 2) <= maxPixelWidth) {
                    std::memcpy(currentLine, candidate, candLen + wordLen + 1);
                    currentLen = candLen + wordLen;
                    continue;
                }
            }

            if (currentLen > 0) {
                if (count < 32) {
                    std::memcpy(lines[count], currentLine, currentLen + 1);
                    count++;
                }
                currentLine[0] = '\0';
                currentLen = 0;
            }

            size_t wIdx = 0;
            while (wIdx < wordLen) {
                char c = text[wordStart + wIdx];
                char testBuf[64];
                std::memcpy(testBuf, currentLine, currentLen);
                testBuf[currentLen] = c;
                testBuf[currentLen + 1] = '\0';

                if (currentLen > 0 && tft.textWidth(testBuf, 2) > maxPixelWidth) {
                    if (count < 32) {
                        std::memcpy(lines[count], currentLine, currentLen + 1);
                        count++;
                    }
                    currentLine[0] = '\0';
                    currentLen = 0;
                }

                if (currentLen < sizeof(currentLine) - 1) {
                    currentLine[currentLen++] = c;
                    currentLine[currentLen] = '\0';
                }
                wIdx++;
            }
        }
    }

    if (currentLen > 0 && count < 32) {
        std::memcpy(lines[count], currentLine, currentLen + 1);
        count++;
    }

    int start = 0;
    if (count > maxVisibleLines) {
        start = count - maxVisibleLines;
    }
    lineCount = 0;
    for (int i = start; i < count; ++i) {
        std::memcpy(outputLines[lineCount], lines[i], 64);
        lineCount++;
    }
}

static void drawStaticLayout() {
    int w = tft.width();
    int h = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    // 0-22: Header area
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("MORSE DECODER", 5, 3, 2);
    tft.drawFastHLine(0, 23, w, TFT_DARKGREY);

    // 26-54: Now area
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Now:", 5, 30, 2);
    tft.drawFastHLine(0, 55, w, TFT_DARKGREY);

    // 58-106: Morse history area
    tft.drawString("Morse:", 5, 58, 2);
    tft.drawFastHLine(0, 107, w, TFT_DARKGREY);

    // 110-206: Text area
    tft.drawString("Text:", 5, 110, 2);
    tft.drawFastHLine(0, 209, w, TFT_DARKGREY);

    // 214-240: Status bar
    tft.drawString("HOLD 2s = CLEAR", 5, 218, 2);

    (void)h;

    // Reset caches to force initial render
    s_lastWpm = 0xFFFFFFFF;
    s_lastPressed = false;
    s_lastSymbols[0] = '\1';
    s_lastMorse[0] = '\1';
    s_lastText[0] = '\1';
    s_layoutReady = true;
}

void displayInit() {
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(TFT_BLACK);
}

void displaySplash() {
    int w = tft.width();
    int h = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("MORSE DECODER", w / 2, h / 2 - 20, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Key Morse with the button", w / 2, h / 2 + 25, 2);
    delay(800);
    buzzerConfirm();
    drawStaticLayout();
}

void displaySelfTest() {
    int w = tft.width();
    int h = tft.height();
    int barH = h / 4;

    tft.fillRect(0, 0, w, barH, TFT_RED);
    tft.fillRect(0, barH, w, barH, TFT_GREEN);
    tft.fillRect(0, 2 * barH, w, barH, TFT_BLUE);
    tft.fillRect(0, 3 * barH, w, h - 3 * barH, TFT_WHITE);
    buzzerOn();
    delay(1000);
    buzzerOff();
    delay(500);
    drawStaticLayout();
}

void displayRender(const morse::Decoder& decoder) {
    if (!s_layoutReady) {
        drawStaticLayout();
    }

    int w = tft.width();
    tft.setTextDatum(TL_DATUM);

    // 1. Header WPM (0-22 px)
    uint32_t currentWpm = decoder.wpm();
    if (currentWpm != s_lastWpm) {
        s_lastWpm = currentWpm;
        char wpmBuf[16];
        std::snprintf(wpmBuf, sizeof(wpmBuf), "~%lu WPM", static_cast<unsigned long>(currentWpm));
        tft.fillRect(w - 95, 0, 95, 23, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(wpmBuf, w - 5, 3, 2);
        tft.setTextDatum(TL_DATUM);
    }

    // 2. Now / Symbols area (26-54 px)
    bool currentPressed = decoder.isPressed();
    const char* currentSymbols = decoder.currentSymbols();
    if (currentPressed != s_lastPressed || std::strcmp(currentSymbols, s_lastSymbols) != 0) {
        s_lastPressed = currentPressed;
        std::strncpy(s_lastSymbols, currentSymbols, sizeof(s_lastSymbols) - 1);
        s_lastSymbols[sizeof(s_lastSymbols) - 1] = '\0';

        tft.fillRect(60, 26, w - 65, 29, TFT_BLACK);
        if (currentPressed) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString("KEYING", 60, 27, 4);
        } else if (currentSymbols[0] != '\0') {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.drawString(currentSymbols, 60, 27, 4);
        }
    }

    // 3. Morse history area (58-106 px, showing newest 2 lines at 74 and 90)
    const char* currentMorse = decoder.morseLine();
    if (std::strcmp(currentMorse, s_lastMorse) != 0) {
        std::strncpy(s_lastMorse, currentMorse, sizeof(s_lastMorse) - 1);
        s_lastMorse[sizeof(s_lastMorse) - 1] = '\0';

        tft.fillRect(5, 74, w - 10, 33, TFT_BLACK);
        char lines[2][64];
        int lineCount = 0;
        wrapText(currentMorse, w - 10, 2, lines, lineCount);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        for (int i = 0; i < lineCount; ++i) {
            tft.drawString(lines[i], 5, 74 + i * 16, 2);
        }
    }

    // 4. Decoded English text area (110-206 px, showing newest lines at 126, 142, 158, 174, 190)
    const char* currentText = decoder.text();
    if (std::strcmp(currentText, s_lastText) != 0) {
        std::strncpy(s_lastText, currentText, sizeof(s_lastText) - 1);
        s_lastText[sizeof(s_lastText) - 1] = '\0';

        tft.fillRect(5, 126, w - 10, 81, TFT_BLACK);
        char lines[5][64];
        int lineCount = 0;
        wrapText(currentText, w - 10, 5, lines, lineCount);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        for (int i = 0; i < lineCount; ++i) {
            tft.drawString(lines[i], 5, 126 + i * 16, 2);
        }
    }
}
