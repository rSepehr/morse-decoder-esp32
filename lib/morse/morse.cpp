#include "morse.h"

namespace morse {

namespace {

struct MorseEntry {
    const char* code;
    char ch;
};

// ITU Morse table
static constexpr MorseEntry kMorseTable[] = {
    {"...", 'S'},
    {"---", 'O'},
    {".-", 'A'},
    {"-...", 'B'},
    {"-.-.", 'C'},
    {"-..", 'D'},
    {".", 'E'},
    {"..-.", 'F'},
    {"--.", 'G'},
    {"....", 'H'},
    {"..", 'I'},
    {".---", 'J'},
    {"-.-", 'K'},
    {".-..", 'L'},
    {"--", 'M'},
    {"-.", 'N'},
    {".--.", 'P'},
    {"--.-", 'Q'},
    {".-.", 'R'},
    {"-", 'T'},
    {"..-", 'U'},
    {"...-", 'V'},
    {".--", 'W'},
    {"-..-", 'X'},
    {"-.--", 'Y'},
    {"--..", 'Z'},
    {"-----", '0'},
    {".----", '1'},
    {"..---", '2'},
    {"...--", '3'},
    {"....-", '4'},
    {".....", '5'},
    {"-....", '6'},
    {"--...", '7'},
    {"---..", '8'},
    {"----.", '9'},
    {".-.-.-", '.'},
    {"--..--", ','},
    {"..--..", '?'},
    {".----.", '\''},
    {"-.-.--", '!'},
    {"-..-.", '/'},
    {"-.--.", '('},
    {"-.--.-", ')'},
    {".-...", '&'},
    {"---...", ':'},
    {"-.-.-.", ';'},
    {"-...-", '='},
    {".-.-.", '+'},
    {"-....-", '-'},
    {"..--.-", '_'},
    {".-..-.", '"'},
    {"...-..-", '$'},
    {".--.-.", '@'}
};

static constexpr size_t kMorseTableSize = sizeof(kMorseTable) / sizeof(kMorseTable[0]);

size_t internalStrLen(const char* s) {
    size_t len = 0;
    while (s != nullptr && s[len] != '\0') {
        len++;
    }
    return len;
}

bool internalStrEqual(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return (a[i] == '\0' && b[i] == '\0');
}

char lookupMorse(const char* symbols) {
    for (size_t i = 0; i < kMorseTableSize; ++i) {
        if (internalStrEqual(symbols, kMorseTable[i].code)) {
            return kMorseTable[i].ch;
        }
    }
    return '\0';
}

void appendToFixedBuffer(char* buf, size_t maxLen, const char* str) {
    size_t curLen = internalStrLen(buf);
    size_t addLen = internalStrLen(str);
    if (curLen + addLen <= maxLen) {
        for (size_t i = 0; i < addLen; ++i) {
            buf[curLen + i] = str[i];
        }
        buf[curLen + addLen] = '\0';
    } else {
        if (addLen >= maxLen) {
            size_t offset = addLen - maxLen;
            for (size_t i = 0; i < maxLen; ++i) {
                buf[i] = str[offset + i];
            }
            buf[maxLen] = '\0';
        } else {
            size_t keep = maxLen - addLen;
            size_t drop = curLen - keep;
            for (size_t i = 0; i < keep; ++i) {
                buf[i] = buf[drop + i];
            }
            for (size_t i = 0; i < addLen; ++i) {
                buf[keep + i] = str[i];
            }
            buf[maxLen] = '\0';
        }
    }
}

void appendCharToFixedBuffer(char* buf, size_t maxLen, char c) {
    char s[2] = {c, '\0'};
    appendToFixedBuffer(buf, maxLen, s);
}

bool twoGroupSplit(const float* values, size_t n, float& mLowOut, float& mHighOut) {
    if (n < 2) {
        return false;
    }

    static constexpr size_t kMaxSplit = 16;
    float arr[kMaxSplit];
    for (size_t i = 0; i < n; ++i) {
        arr[i] = values[i];
    }
    for (size_t i = 1; i < n; ++i) {
        float key = arr[i];
        size_t j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }

    // 2. For each split index k from 1 to n-1, compute within-group squared deviations
    float bestError = -1.0f;
    float bestMLow = 0.0f;
    float bestMHigh = 0.0f;

    for (size_t k = 1; k < n; ++k) {
        float sumLow = 0.0f;
        for (size_t i = 0; i < k; ++i) {
            sumLow += arr[i];
        }
        float mLow = sumLow / static_cast<float>(k);
        float ssLow = 0.0f;
        for (size_t i = 0; i < k; ++i) {
            float diff = arr[i] - mLow;
            ssLow += diff * diff;
        }

        float sumHigh = 0.0f;
        for (size_t i = k; i < n; ++i) {
            sumHigh += arr[i];
        }
        float mHigh = sumHigh / static_cast<float>(n - k);
        float ssHigh = 0.0f;
        for (size_t i = k; i < n; ++i) {
            float diff = arr[i] - mHigh;
            ssHigh += diff * diff;
        }

        float totalError = ssLow + ssHigh;
        // 3. Choose k with smallest total. On tie, choose smaller k.
        if (k == 1 || totalError < bestError) {
            bestError = totalError;
            bestMLow = mLow;
            bestMHigh = mHigh;
        }
    }

    mLowOut = bestMLow;
    mHighOut = bestMHigh;
    // 4. Valid only if mHigh >= 2.0 * mLow
    return (bestMHigh >= 2.0f * bestMLow);
}

} // namespace

Decoder::Decoder(uint32_t initialUnitMs, uint32_t longPressMs)
    : initialUnitMs_(initialUnitMs == 0 ? 150 : initialUnitMs),
      longPressMs_(longPressMs == 0 ? 2000 : longPressMs),
      unit_(static_cast<float>(initialUnitMs_)),
      threshold_(2.0f * unit_),
      intraMean_(unit_),
      letterMean_(3.0f * unit_),
      wordMean_(7.0f * unit_),
      boundaryL_(2.5f * unit_),
      boundaryW_(3.5f * unit_),
      pressCount_(0),
      pressHead_(0),
      gapCount_(0),
      gapHead_(0),
      pressed_(false),
      pressStartMs_(0),
      lastReleaseMs_(0),
      hasReleaseSinceClear_(false),
      letterCommitted_(true),
      wordCommitted_(true),
      longPressConsumed_(false),
      overflow_(false),
      symbolCount_(0),
      revision_(0) {
    for (size_t i = 0; i < kWindow; ++i) {
        pressWindow_[i] = 0;
        gapWindow_[i] = 0.0f;
    }
    symbols_[0] = '\0';
    morseLine_[0] = '\0';
    text_[0] = '\0';
    recomputeBoundaries();
}

void Decoder::recomputeBoundaries() {
    if (letterMean_ < 2.5f * unit_) letterMean_ = 2.5f * unit_;
    if (letterMean_ > 10.0f * unit_) letterMean_ = 10.0f * unit_;
    if (wordMean_ < 3.0f * unit_) wordMean_ = 3.0f * unit_;
    if (wordMean_ > 20.0f * unit_) wordMean_ = 20.0f * unit_;

    boundaryL_ = intraMean_ + (letterMean_ - intraMean_) / 2.0f;
    if (boundaryL_ < 2.5f * unit_) boundaryL_ = 2.5f * unit_;
    if (boundaryL_ > 4.0f * unit_) boundaryL_ = 4.0f * unit_;

    boundaryW_ = letterMean_ + (wordMean_ - letterMean_) / 2.0f;
    if (boundaryW_ < boundaryL_ + unit_) boundaryW_ = boundaryL_ + unit_;
    if (boundaryW_ > 12.0f * unit_) boundaryW_ = 12.0f * unit_;
}

void Decoder::clear() {
    symbols_[0] = '\0';
    symbolCount_ = 0;
    overflow_ = false;
    morseLine_[0] = '\0';
    text_[0] = '\0';
    letterCommitted_ = true;
    wordCommitted_ = true;
    hasReleaseSinceClear_ = false;
    revision_++;
}

const char* Decoder::currentSymbols() const {
    return symbols_;
}

const char* Decoder::morseLine() const {
    return morseLine_;
}

const char* Decoder::text() const {
    return text_;
}

uint32_t Decoder::unitMs() const {
    return static_cast<uint32_t>(std::round(unit_));
}

uint32_t Decoder::wpm() const {
    if (unit_ <= 0.0f) return 0;
    return static_cast<uint32_t>(std::round(1200.0f / unit_));
}

bool Decoder::isPressed() const {
    return pressed_;
}

uint32_t Decoder::revision() const {
    return revision_;
}

void Decoder::addPressToWindow(uint32_t d) {
    pressWindow_[pressHead_] = d;
    pressHead_ = (pressHead_ + 1) % kWindow;
    if (pressCount_ < kWindow) {
        pressCount_++;
    }

    if (pressCount_ >= 2) {
        float arr[kWindow];
        for (size_t i = 0; i < pressCount_; ++i) {
            arr[i] = static_cast<float>(pressWindow_[i]);
        }
        float mLow = 0.0f;
        float mHigh = 0.0f;
        if (twoGroupSplit(arr, pressCount_, mLow, mHigh)) {
            float newU = (mLow + mHigh / 3.0f) / 2.0f;
            if (newU < kMinUnitMs) newU = kMinUnitMs;
            if (newU > kMaxUnitMs) newU = kMaxUnitMs;
            if (unit_ > 0.0f) {
                float ratio = newU / unit_;
                intraMean_ *= ratio;
                letterMean_ *= ratio;
                wordMean_ *= ratio;
                for (size_t i = 0; i < gapCount_; ++i) {
                    gapWindow_[i] *= ratio;
                }
            }
            unit_ = newU;
            threshold_ = 2.0f * unit_;
            recomputeBoundaries();
        }
    }
}

void Decoder::learnGap(uint32_t nowMs) {
    if (!hasReleaseSinceClear_) {
        return;
    }

    uint32_t g = nowMs - lastReleaseMs_;
    float gFloat = static_cast<float>(g);
    if (gFloat > 20.0f * unit_) {
        return;
    }

    if (gFloat < boundaryL_) {
        // Intra-letter gap: accept only if >= 0.5 * U
        if (gFloat >= 0.5f * unit_) {
            intraMean_ += 0.2f * (gFloat - intraMean_);
            recomputeBoundaries();
        }
    } else {
        // Gap >= boundaryL_: add to gapWindow (keep newest 16)
        gapWindow_[gapHead_] = gFloat;
        gapHead_ = (gapHead_ + 1) % kWindow;
        if (gapCount_ < kWindow) {
            gapCount_++;
        }

        float arr[kWindow];
        for (size_t i = 0; i < gapCount_; ++i) {
            arr[i] = gapWindow_[i];
        }

        float mLow = 0.0f;
        float mHigh = 0.0f;
        if (gapCount_ >= 2 && twoGroupSplit(arr, gapCount_, mLow, mHigh)) {
            letterMean_ = mLow;
            wordMean_ = mHigh;
        } else {
            float sum = 0.0f;
            for (size_t i = 0; i < gapCount_; ++i) {
                sum += arr[i];
            }
            float winMean = sum / static_cast<float>(gapCount_);
            if (winMean < 5.0f * unit_) {
                letterMean_ = winMean;
            }
        }
        recomputeBoundaries();
    }
}

Event Decoder::evaluateGaps(uint32_t nowMs) {
    if (pressed_) {
        return Event::None;
    }

    uint32_t g = nowMs - lastReleaseMs_;
    float gFloat = static_cast<float>(g);
    Event lastEvent = Event::None;

    // Rule A (letter): if g >= L and letter not yet committed, commit letter
    if (gFloat >= boundaryL_ && !letterCommitted_) {
        letterCommitted_ = true;
        if (symbolCount_ > 0) {
            if (morseLine_[0] != '\0') {
                size_t mlen = internalStrLen(morseLine_);
                if (morseLine_[mlen - 1] != ' ') {
                    appendCharToFixedBuffer(morseLine_, kMaxHistory, ' ');
                }
            }
            appendToFixedBuffer(morseLine_, kMaxHistory, symbols_);

            if (overflow_) {
                appendCharToFixedBuffer(text_, kMaxHistory, '?');
                lastEvent = Event::Error;
            } else {
                char ch = lookupMorse(symbols_);
                if (ch != '\0') {
                    appendCharToFixedBuffer(text_, kMaxHistory, ch);
                    lastEvent = Event::Letter;
                } else {
                    appendCharToFixedBuffer(text_, kMaxHistory, '?');
                    lastEvent = Event::Error;
                }
            }

            symbols_[0] = '\0';
            symbolCount_ = 0;
            overflow_ = false;
            revision_++;
        }
    }

    // Rule B (word): if g >= W and word not yet committed, apply word boundary
    if (gFloat >= boundaryW_ && !wordCommitted_) {
        wordCommitted_ = true;
        bool changed = false;

        if (morseLine_[0] != '\0') {
            size_t mlen = internalStrLen(morseLine_);
            char last = morseLine_[mlen - 1];
            if (last != ' ' && last != '/') {
                appendToFixedBuffer(morseLine_, kMaxHistory, " /");
                changed = true;
            }
        }

        if (text_[0] != '\0') {
            size_t tlen = internalStrLen(text_);
            if (text_[tlen - 1] != ' ') {
                appendCharToFixedBuffer(text_, kMaxHistory, ' ');
                changed = true;
            }
        }

        if (changed) {
            revision_++;
        }
        lastEvent = Event::WordSpace;
    }

    return lastEvent;
}

Event Decoder::onPress(uint32_t nowMs) {
    learnGap(nowMs);
    evaluateGaps(nowMs);
    pressed_ = true;
    pressStartMs_ = nowMs;
    longPressConsumed_ = false;
    revision_++;
    return Event::None;
}

Event Decoder::onRelease(uint32_t nowMs) {
    if (!pressed_) {
        return Event::None;
    }

    pressed_ = false;
    lastReleaseMs_ = nowMs;
    hasReleaseSinceClear_ = true;
    letterCommitted_ = false;
    wordCommitted_ = false;

    if (longPressConsumed_) {
        longPressConsumed_ = false;
        return Event::None;
    }

    uint32_t d = nowMs - pressStartMs_;
    if (d >= longPressMs_) {
        clear();
        return Event::Clear;
    }

    if (d < kMinPressMs) {
        return Event::None;
    }

    Event ev;
    char sym;
    if (static_cast<float>(d) < threshold_) {
        ev = Event::Dot;
        sym = '.';
    } else {
        ev = Event::Dash;
        sym = '-';
    }

    if (symbolCount_ < kMaxSymbols) {
        symbols_[symbolCount_++] = sym;
        symbols_[symbolCount_] = '\0';
    } else {
        overflow_ = true;
    }
    revision_++;

    addPressToWindow(d);
    return ev;
}

Event Decoder::poll(uint32_t nowMs) {
    if (pressed_) {
        if (!longPressConsumed_ && (nowMs - pressStartMs_ >= longPressMs_)) {
            clear();
            longPressConsumed_ = true;
            return Event::Clear;
        }
        return Event::None;
    }
    return evaluateGaps(nowMs);
}

} // namespace morse
