#ifndef MORSE_DECODER_H
#define MORSE_DECODER_H

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace morse {

enum class Event : uint8_t {
    None,
    Dot,
    Dash,
    Letter,
    WordSpace,
    Error,
    Clear
};

class Decoder {
public:
    Decoder(uint32_t initialUnitMs, uint32_t longPressMs);

    Event onPress(uint32_t nowMs);
    Event onRelease(uint32_t nowMs);
    Event poll(uint32_t nowMs);
    void clear();

    const char* currentSymbols() const;
    const char* morseLine() const;
    const char* text() const;
    uint32_t unitMs() const;
    uint32_t wpm() const;
    bool isPressed() const;
    uint32_t revision() const;

private:
    static constexpr uint32_t kMinPressMs = 25;
    static constexpr size_t kWindow = 16;
    static constexpr float kMinUnitMs = 20.0f;
    static constexpr float kMaxUnitMs = 1000.0f;
    static constexpr size_t kMaxSymbols = 6;
    static constexpr size_t kMaxHistory = 256;

    Event evaluateGaps(uint32_t nowMs);
    void addPressToWindow(uint32_t d);
    void learnGap(uint32_t nowMs);
    void recomputeBoundaries();

    uint32_t initialUnitMs_;
    uint32_t longPressMs_;

    float unit_;
    float threshold_;

    float intraMean_;
    float letterMean_;
    float wordMean_;

    float boundaryL_;
    float boundaryW_;

    uint32_t pressWindow_[kWindow];
    size_t pressCount_;
    size_t pressHead_;

    float gapWindow_[kWindow];
    size_t gapCount_;
    size_t gapHead_;

    bool pressed_;
    uint32_t pressStartMs_;
    uint32_t lastReleaseMs_;
    bool hasReleaseSinceClear_;

    bool letterCommitted_;
    bool wordCommitted_;
    bool longPressConsumed_;
    bool overflow_;

    size_t symbolCount_;
    char symbols_[kMaxSymbols + 1];
    char morseLine_[kMaxHistory + 1];
    char text_[kMaxHistory + 1];

    uint32_t revision_;
};

} // namespace morse

#endif // MORSE_DECODER_H
