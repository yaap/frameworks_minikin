/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef MINIKIN_FONT_VARIATION_H
#define MINIKIN_FONT_VARIATION_H

#include <cstdint>
#include <iostream>

#include "minikin/SortedPackedVector.h"

namespace minikin {

typedef uint32_t AxisTag;

struct FontVariation {
    FontVariation() = default;
    FontVariation(AxisTag axisTag, float value) : axisTag(axisTag), value(value) {}
    AxisTag axisTag;
    float value;
};

constexpr bool operator==(const FontVariation& l, const FontVariation& r) {
    return l.axisTag == r.axisTag && l.value == r.value;
}

constexpr bool operator!=(const FontVariation& l, const FontVariation& r) {
    return !(l == r);
}

constexpr bool operator<(const FontVariation& l, const FontVariation& r) {
    return l.axisTag < r.axisTag;
}

constexpr bool operator>(const FontVariation& l, const FontVariation& r) {
    return l.axisTag > r.axisTag;
}

constexpr bool operator<=(const FontVariation& l, const FontVariation& r) {
    return l.axisTag <= r.axisTag;
}

constexpr bool operator>=(const FontVariation& l, const FontVariation& r) {
    return l.axisTag >= r.axisTag;
}

// Immutable variation settings
using VariationSettings = SortedPackedVector<FontVariation, 2, uint16_t>;

inline std::ostream& operator<<(std::ostream& os, const FontVariation& variation) {
    return os << "'" << static_cast<char>(variation.axisTag >> 24)
              << static_cast<char>(variation.axisTag >> 16)
              << static_cast<char>(variation.axisTag >> 8) << static_cast<char>(variation.axisTag)
              << "' " << variation.value;
}

inline std::ostream& operator<<(std::ostream& os, const VariationSettings& varSettings) {
    for (size_t i = 0; i < varSettings.size(); ++i) {
        if (i != 0) {
            os << ", ";
        }
        os << varSettings[i];
    }
    return os;
}

}  // namespace minikin

#endif  // MINIKIN_FONT_VARIATION_H
