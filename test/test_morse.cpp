#include <unity.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include "morse.h"

// Deterministic Linear Congruential Generator for jitter
class JitterLcg {
public:
    explicit JitterLcg(uint32_t seed = 42) : state_(seed) {}
    void reset(uint32_t seed = 42) { state_ = seed; }

    uint32_t apply(uint32_t nominalMs) {
        state_ = state_ * 1664525u + 1013904223u;
        // Map to -0.15 .. +0.15 (+/- 15%)
        double frac = static_cast<double>(state_) / 4294967296.0;
        double jitter = 1.0 + (frac * 0.30 - 0.15);
        double val = static_cast<double>(nominalMs) * jitter;
        if (val < 1.0) val = 1.0;
        return static_cast<uint32_t>(std::round(val));
    }

private:
    uint32_t state_;
};

// Drive Decoder by calling poll() every 5 ms of simulated time
static void advanceSimTime(morse::Decoder& dec, uint32_t& simNow, uint32_t durationMs) {
    uint32_t target = simNow + durationMs;
    while (simNow + 5 <= target) {
        simNow += 5;
        dec.poll(simNow);
    }
    if (simNow < target) {
        simNow = target;
        dec.poll(simNow);
    }
}

static void doPress(morse::Decoder& dec, uint32_t& simNow, uint32_t pressMs) {
    dec.onPress(simNow);
    advanceSimTime(dec, simNow, pressMs);
    dec.onRelease(simNow);
}

static void doGap(morse::Decoder& dec, uint32_t& simNow, uint32_t gapMs) {
    advanceSimTime(dec, simNow, gapMs);
}

static const char* charToMorse(char c) {
    switch (c) {
        case 'A': return ".-";
        case 'B': return "-...";
        case 'C': return "-.-.";
        case 'D': return "-..";
        case 'E': return ".";
        case 'F': return "..-.";
        case 'G': return "--.";
        case 'H': return "....";
        case 'I': return "..";
        case 'J': return ".---";
        case 'K': return "-.-";
        case 'L': return ".-..";
        case 'M': return "--";
        case 'N': return "-.";
        case 'O': return "---";
        case 'P': return ".--.";
        case 'Q': return "--.-";
        case 'R': return ".-.";
        case 'S': return "...";
        case 'T': return "-";
        case 'U': return "..-";
        case 'V': return "...-";
        case 'W': return ".--";
        case 'X': return "-..-";
        case 'Y': return "-.--";
        case 'Z': return "--..";
        case '0': return "-----";
        case '1': return ".----";
        case '2': return "..---";
        case '3': return "...--";
        case '4': return "....-";
        case '5': return ".....";
        case '6': return "-....";
        case '7': return "--...";
        case '8': return "---..";
        case '9': return "----.";
        case '.': return ".-.-.-";
        case ',': return "--..--";
        case '?': return "..--..";
        case '\'': return ".----.";
        case '!': return "-.-.--";
        case '/': return "-..-.";
        case '(': return "-.--.";
        case ')': return "-.--.-";
        case '&': return ".-...";
        case ':': return "---...";
        case ';': return "-.-.-.";
        case '=': return "-...-";
        case '+': return ".-.-.";
        case '-': return "-....-";
        case '_': return "..--.-";
        case '"': return ".-..-.";
        case '$': return "...-..-";
        case '@': return ".--.-.";
        default: return "";
    }
}

static void feedMorse(morse::Decoder& dec, uint32_t& simNow, const char* str, uint32_t unitMs,
                      JitterLcg& lcg, bool withJitter,
                      float intraGapUnits = 1.0f,
                      float letterGapUnits = 3.0f,
                      float wordGapUnits = 7.0f,
                      float finalIdleUnits = 7.0f) {
    size_t len = std::strlen(str);

    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        if (c == ' ') {
            uint32_t gap = static_cast<uint32_t>(std::round(wordGapUnits * static_cast<float>(unitMs)));
            if (withJitter) gap = lcg.apply(gap);
            doGap(dec, simNow, gap);
            continue;
        }

        const char* code = charToMorse(c);
        size_t codeLen = std::strlen(code);

        for (size_t s = 0; s < codeLen; ++s) {
            char sym = code[s];
            uint32_t pressDur = (sym == '-') ? (3 * unitMs) : (1 * unitMs);
            if (withJitter) pressDur = lcg.apply(pressDur);
            doPress(dec, simNow, pressDur);

            if (s + 1 < codeLen) {
                uint32_t gap = static_cast<uint32_t>(std::round(intraGapUnits * static_cast<float>(unitMs)));
                if (withJitter) gap = lcg.apply(gap);
                doGap(dec, simNow, gap);
            }
        }

        if (i + 1 < len) {
            char nextC = str[i + 1];
            if (nextC != ' ') {
                uint32_t gap = static_cast<uint32_t>(std::round(letterGapUnits * static_cast<float>(unitMs)));
                if (withJitter) gap = lcg.apply(gap);
                doGap(dec, simNow, gap);
            }
        } else {
            uint32_t gap = static_cast<uint32_t>(std::round(finalIdleUnits * static_cast<float>(unitMs)));
            if (withJitter) gap = lcg.apply(gap);
            doGap(dec, simNow, gap);
        }
    }
}

static void trimTrailingSpaces(char* str) {
    if (!str) return;
    int len = static_cast<int>(std::strlen(str));
    while (len > 0 && str[len - 1] == ' ') {
        str[len - 1] = '\0';
        len--;
    }
}

void setUp(void) {}
void tearDown(void) {}

// 1. Classification on a fresh decoder: a 100 ms press is Dot, a 400 ms press is Dash.
void test_classification_fresh(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    dec.onPress(simNow);
    simNow += 100;
    morse::Event ev1 = dec.onRelease(simNow);
    TEST_ASSERT_EQUAL(morse::Event::Dot, ev1);

    simNow += 50;

    dec.onPress(simNow);
    simNow += 400;
    morse::Event ev2 = dec.onRelease(simNow);
    TEST_ASSERT_EQUAL(morse::Event::Dash, ev2);
}

// 2. A single "E" (one dot followed by a letter gap) decodes to "E".
void test_single_e(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    dec.onPress(simNow);
    advanceSimTime(dec, simNow, 100);
    dec.onRelease(simNow);

    // Initial unit = 150 ms, L clamped to [2.5*U, 4.0*U] = [375, 600]. Letter gap 3U = 450 ms.
    advanceSimTime(dec, simNow, 450);

    TEST_ASSERT_EQUAL_STRING("E", dec.text());
}

// 3. "SOS HELP" decodes correctly at 5, 10, 15, 20 and 25 WPM with jitter.
void test_sos_help_at_speeds(void) {
    const uint32_t speeds[] = {5, 10, 15, 20, 25};

    for (size_t idx = 0; idx < sizeof(speeds) / sizeof(speeds[0]); ++idx) {
        uint32_t wpm = speeds[idx];
        uint32_t unitMs = 1200 / wpm;
        morse::Decoder dec(150, 2000);
        uint32_t simNow = 1000;
        JitterLcg lcg(wpm * 100 + 42);

        // Warm up with "PARIS PARIS "
        feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, true);
        dec.clear();

        // Feed "SOS HELP"
        feedMorse(dec, simNow, "SOS HELP", unitMs, lcg, true);

        char buf[128];
        std::strncpy(buf, dec.text(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        trimTrailingSpaces(buf);

        TEST_ASSERT_EQUAL_STRING("SOS HELP", buf);
    }
}

// 4. "HELLO WORLD 73" decodes correctly with the same warm-up at 5-25 WPM with jitter.
void test_hello_world_73(void) {
    const uint32_t speeds[] = {5, 10, 15, 20, 25};

    for (size_t idx = 0; idx < sizeof(speeds) / sizeof(speeds[0]); ++idx) {
        uint32_t wpm = speeds[idx];
        uint32_t unitMs = 1200 / wpm;
        morse::Decoder dec(150, 2000);
        uint32_t simNow = 1000;
        JitterLcg lcg(wpm * 200 + 77);

        // Warm up with "PARIS PARIS "
        feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, true);
        dec.clear();

        // Feed "HELLO WORLD 73"
        feedMorse(dec, simNow, "HELLO WORLD 73", unitMs, lcg, true);

        char buf[128];
        std::strncpy(buf, dec.text(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        trimTrailingSpaces(buf);

        TEST_ASSERT_EQUAL_STRING("HELLO WORLD 73", buf);
    }
}

// 5. Speed estimation: after warm-up at 12 WPM, wpm() returns a value between 10 and 14.
void test_speed_estimation_12wpm(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;
    JitterLcg lcg(12345);
    uint32_t unitMs = 1200 / 12; // 100 ms

    feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, true);

    uint32_t estimatedWpm = dec.wpm();
    TEST_ASSERT_TRUE(estimatedWpm >= 10 && estimatedWpm <= 14);
}

// 6. Invalid sequence: 8 dots followed by a letter gap produce Error, and the text ends with "?".
void test_invalid_sequence_8_dots(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    for (int i = 0; i < 8; ++i) {
        dec.onPress(simNow);
        advanceSimTime(dec, simNow, 100);
        dec.onRelease(simNow);
        if (i < 7) {
            advanceSimTime(dec, simNow, 100);
        }
    }

    // Letter gap >= L (450 ms)
    advanceSimTime(dec, simNow, 450);

    size_t len = std::strlen(dec.text());
    TEST_ASSERT_TRUE(len > 0);
    TEST_ASSERT_EQUAL('?', dec.text()[len - 1]);
}

// 7. Morse line: after clear(), keying "HI THERE" (with the same jitter) produces the morse line exactly ".... .. / - .... . .-. .".
void test_morse_line_hi_there(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;
    JitterLcg lcg(999);
    uint32_t unitMs = 100; // 12 WPM

    // Warm up
    feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, true);
    dec.clear();
    lcg.reset(999);

    // Key "HI THERE" with 3U final idle so 'E' is committed, without trailing word boundary
    feedMorse(dec, simNow, "HI THERE", unitMs, lcg, true, 1.0f, 3.0f, 7.0f, 3.0f);

    TEST_ASSERT_EQUAL_STRING(".... .. / - .... . .-. .", dec.morseLine());
}

// 8. Long press: a press of 2100 ms produces Clear. Text and Morse line are empty afterwards, and releasing adds no symbol.
void test_long_press_clear(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    // Key 'E'
    dec.onPress(simNow);
    advanceSimTime(dec, simNow, 100);
    dec.onRelease(simNow);
    advanceSimTime(dec, simNow, 450);
    TEST_ASSERT_EQUAL_STRING("E", dec.text());

    // Long press 2100 ms
    dec.onPress(simNow);
    advanceSimTime(dec, simNow, 2100);

    TEST_ASSERT_EQUAL_STRING("", dec.text());
    TEST_ASSERT_EQUAL_STRING("", dec.morseLine());

    // Release
    morse::Event ev = dec.onRelease(simNow);
    TEST_ASSERT_EQUAL(morse::Event::None, ev);
    TEST_ASSERT_EQUAL_STRING("", dec.currentSymbols());
    TEST_ASSERT_EQUAL_STRING("", dec.text());
    TEST_ASSERT_EQUAL_STRING("", dec.morseLine());
}

// 9. A 10 ms press is ignored: no symbol is added and currentSymbols() stays empty.
void test_short_press_ignored(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    dec.onPress(simNow);
    advanceSimTime(dec, simNow, 10);
    morse::Event ev = dec.onRelease(simNow);

    TEST_ASSERT_EQUAL(morse::Event::None, ev);
    TEST_ASSERT_EQUAL_STRING("", dec.currentSymbols());
}

// 10. Slow letter "I" on a fresh decoder, no jitter: dot 150 ms, intra gap 330 ms, dot 150 ms, then a letter gap of 450 ms. Expected text: "I".
void test_slow_letter_i(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;

    doPress(dec, simNow, 150);
    doGap(dec, simNow, 330);
    doPress(dec, simNow, 150);
    doGap(dec, simNow, 450);

    TEST_ASSERT_EQUAL_STRING("I", dec.text());
}

// 11. Slow intra gaps: warm up at 12 WPM with intra-gap units of 1.6, then clear(). Key "SOS" with the same spacing, no jitter. Expected text: "SOS".
void test_slow_intra_gaps_sos(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;
    JitterLcg lcg(123);
    uint32_t unitMs = 1200 / 12; // 100 ms

    feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, false, 1.6f, 3.0f, 7.0f, 7.0f);
    dec.clear();

    feedMorse(dec, simNow, "SOS", unitMs, lcg, false, 1.6f, 3.0f, 7.0f, 7.0f);

    char buf[64];
    std::strncpy(buf, dec.text(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trimTrailingSpaces(buf);

    TEST_ASSERT_EQUAL_STRING("SOS", buf);
}

// 12. Farnsworth spacing: warm up with "PARIS PARIS " using letter-gap units of 6 and word-gap units of 14, then clear(). Key "HELLO WORLD" with the same spacing, no jitter. Expected text: "HELLO WORLD".
void test_farnsworth_spacing(void) {
    morse::Decoder dec(150, 2000);
    uint32_t simNow = 1000;
    JitterLcg lcg(456);
    uint32_t unitMs = 1200 / 12; // 100 ms

    feedMorse(dec, simNow, "PARIS PARIS ", unitMs, lcg, false, 1.0f, 6.0f, 14.0f, 14.0f);
    dec.clear();

    feedMorse(dec, simNow, "HELLO WORLD", unitMs, lcg, false, 1.0f, 6.0f, 14.0f, 14.0f);

    char buf[64];
    std::strncpy(buf, dec.text(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trimTrailingSpaces(buf);

    TEST_ASSERT_EQUAL_STRING("HELLO WORLD", buf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_classification_fresh);
    RUN_TEST(test_single_e);
    RUN_TEST(test_sos_help_at_speeds);
    RUN_TEST(test_hello_world_73);
    RUN_TEST(test_speed_estimation_12wpm);
    RUN_TEST(test_invalid_sequence_8_dots);
    RUN_TEST(test_morse_line_hi_there);
    RUN_TEST(test_long_press_clear);
    RUN_TEST(test_short_press_ignored);
    RUN_TEST(test_slow_letter_i);
    RUN_TEST(test_slow_intra_gaps_sos);
    RUN_TEST(test_farnsworth_spacing);
    return UNITY_END();
}
