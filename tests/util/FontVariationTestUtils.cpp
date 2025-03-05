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

#include <hb.h>

#include <vector>

#include "StringPiece.h"
#include "minikin/FontVariation.h"

namespace minikin {

VariationSettings parseVariationSettings(const std::string& varSettings) {
    std::vector<FontVariation> variations;

    SplitIterator it(varSettings, ',');
    while (it.hasNext()) {
        StringPiece var = it.next();

        static hb_variation_t variation;
        if (hb_variation_from_string(var.data(), var.size(), &variation)) {
            variations.push_back({static_cast<AxisTag>(variation.tag), variation.value});
        }
    }
    return VariationSettings(variations);
}

}  // namespace minikin
