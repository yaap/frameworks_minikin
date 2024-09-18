/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINIKIN_FONT_FAKERY_H
#define MINIKIN_FONT_FAKERY_H

namespace minikin {

// attributes representing transforms (fake bold, fake italic) to match styles
class FontFakery {
public:
    FontFakery() : FontFakery(false, false, -1, -1) {}
    FontFakery(bool fakeBold, bool fakeItalic) : FontFakery(fakeBold, fakeItalic, -1, -1) {}
    FontFakery(bool fakeBold, bool fakeItalic, int16_t wghtAdjustment, int8_t italAdjustment)
            : mBits(pack(fakeBold, fakeItalic, wghtAdjustment, italAdjustment)) {}

    // TODO: want to support graded fake bolding
    bool isFakeBold() { return (mBits & MASK_FAKE_BOLD) != 0; }
    bool isFakeItalic() { return (mBits & MASK_FAKE_ITALIC) != 0; }
    bool hasAdjustment() const { return hasWghtAdjustment() || hasItalAdjustment(); }
    bool hasWghtAdjustment() const { return (mBits & MASK_HAS_WGHT_ADJUSTMENT) != 0; }
    bool hasItalAdjustment() const { return (mBits & MASK_HAS_ITAL_ADJUSTMENT) != 0; }
    int16_t wghtAdjustment() const {
        if (hasWghtAdjustment()) {
            return (mBits & MASK_WGHT_ADJUSTMENT) >> WGHT_ADJUSTMENT_SHIFT;
        } else {
            return -1;
        }
    }

    int8_t italAdjustment() const {
        if (hasItalAdjustment()) {
            return (mBits & MASK_ITAL_ADJUSTMENT) != 0 ? 1 : 0;
        } else {
            return -1;
        }
    }

    uint16_t bits() const { return mBits; }

    inline bool operator==(const FontFakery& o) const { return mBits == o.mBits; }
    inline bool operator!=(const FontFakery& o) const { return !(*this == o); }

private:
    static constexpr uint16_t MASK_FAKE_BOLD = 1u;
    static constexpr uint16_t MASK_FAKE_ITALIC = 1u << 1;
    static constexpr uint16_t MASK_HAS_WGHT_ADJUSTMENT = 1u << 2;
    static constexpr uint16_t MASK_HAS_ITAL_ADJUSTMENT = 1u << 3;
    static constexpr uint16_t MASK_ITAL_ADJUSTMENT = 1u << 4;
    static constexpr uint16_t MASK_WGHT_ADJUSTMENT = 0b1111111111u << 5;
    static constexpr uint16_t WGHT_ADJUSTMENT_SHIFT = 5;

    uint16_t pack(bool isFakeBold, bool isFakeItalic, int16_t wghtAdjustment,
                  int8_t italAdjustment) {
        uint16_t bits = 0u;
        bits |= isFakeBold ? MASK_FAKE_BOLD : 0;
        bits |= isFakeItalic ? MASK_FAKE_ITALIC : 0;
        if (wghtAdjustment != -1) {
            bits |= MASK_HAS_WGHT_ADJUSTMENT;
            bits |= (static_cast<uint16_t>(wghtAdjustment) << WGHT_ADJUSTMENT_SHIFT) &
                    MASK_WGHT_ADJUSTMENT;
        }
        if (italAdjustment != -1) {
            bits |= MASK_HAS_ITAL_ADJUSTMENT;
            bits |= (italAdjustment == 1) ? MASK_ITAL_ADJUSTMENT : 0;
        }
        return bits;
    }

    const uint16_t mBits;
};

}  // namespace minikin

#endif  // MINIKIN_FONT_FAKERY_H
