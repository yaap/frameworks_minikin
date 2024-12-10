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

#include "minikin/FontFakery.h"

#include "minikin/Constants.h"
#include "minikin/FVarTable.h"
#include "minikin/FontStyle.h"
#include "minikin/FontVariation.h"

namespace minikin {

FontFakery merge(const FVarTable& fvar, const VariationSettings& baseVS,
                 const VariationSettings& targetVS, FontStyle baseStyle, FontStyle targetStyle) {
    const bool hasItal = fvar.count(TAG_ital);
    const bool hasSlnt = fvar.count(TAG_slnt);
    const bool hasWght = fvar.count(TAG_wght);

    // Reserve size of base and target plus 2 which is the upper bounds resolved axes.
    FontVariation* adjustedVars;
    constexpr uint32_t FIXED_BUFFER_SIZE = 8;
    FontVariation fixedBuffer[FIXED_BUFFER_SIZE];
    std::unique_ptr<FontVariation[]> heap;
    if (baseVS.size() + targetVS.size() + 2 > FIXED_BUFFER_SIZE) {
        heap = std::make_unique<FontVariation[]>(baseVS.size() + targetVS.size() + 2);
        adjustedVars = heap.get();
    } else {
        adjustedVars = fixedBuffer;
    }

    // Convert target font style into font variation settings.
    FontVariation styleVars[2];
    uint32_t styleVarsSize = 0;
    if (hasSlnt) {
        if (targetStyle.slant() == FontStyle::Slant::ITALIC) {
            styleVars[styleVarsSize++] = {TAG_slnt, -10};
        } else {
            styleVars[styleVarsSize++] = {TAG_slnt, 0};
        }
    } else if (hasItal) {
        if (targetStyle.slant() == FontStyle::Slant::ITALIC) {
            styleVars[styleVarsSize++] = {TAG_ital, 1};
        } else {
            styleVars[styleVarsSize++] = {TAG_ital, 0};
        }
    }
    if (hasWght) {
        styleVars[styleVarsSize++] = {TAG_wght, static_cast<float>(targetStyle.weight())};
    }

    // Main merge loop: do the three sorted array merge.
    constexpr uint32_t END = 0xFFFFFFFF;
    bool fakeBold;
    uint32_t baseIdx = 0;
    uint32_t targetIdx = 0;
    uint32_t styleIdx = 0;

    uint32_t adjustedHead = 0;  // head of the output vector.
    while (baseIdx < baseVS.size() || targetIdx < targetVS.size() || styleIdx < styleVarsSize) {
        const AxisTag baseTag = baseIdx < baseVS.size() ? baseVS[baseIdx].axisTag : END;
        const AxisTag targetTag = targetIdx < targetVS.size() ? targetVS[targetIdx].axisTag : END;
        const AxisTag styleTag = styleIdx < styleVarsSize ? styleVars[styleIdx].axisTag : END;

        AxisTag tag;
        float value;
        bool styleValueUsed = false;
        if (baseTag < targetTag) {
            if (styleTag < baseTag) {
                // style < base < target: only process style.
                tag = styleTag;
                value = styleVars[styleIdx].value;
                styleValueUsed = true;
                styleIdx++;
            } else if (styleTag == baseTag) {
                // style == base < target: process base and style. base is used.
                tag = styleTag;
                value = baseVS[baseIdx].value;
                baseIdx++;
                styleIdx++;
            } else {
                //  base < style < target: only process base.
                tag = baseTag;
                value = baseVS[baseIdx].value;
                baseIdx++;
            }
        } else if (targetTag < baseTag) {
            if (styleTag < targetTag) {
                // style < target < base: process style only.
                tag = styleTag;
                value = styleVars[styleIdx].value;
                styleValueUsed = true;
                styleIdx++;
            } else if (styleTag == targetTag) {
                // style = target < base: process style and target. target is used.
                tag = targetTag;
                value = targetVS[targetIdx].value;
                styleIdx++;
                targetIdx++;
            } else {
                // target < style < base: process target only.
                tag = targetTag;
                value = targetVS[targetIdx].value;
                targetIdx++;
            }
        } else {
            if (styleTag < baseTag) {
                // style < base == target: only process style.
                tag = styleTag;
                value = styleVars[styleIdx].value;
                styleValueUsed = true;
                styleIdx++;
            } else if (styleTag == baseTag) {
                //  base == target == style: process all. target is used.
                tag = targetTag;
                value = targetVS[targetIdx].value;
                baseIdx++;
                targetIdx++;
                styleIdx++;
            } else {
                //  base == target < style: process base and target. target is used.
                tag = targetTag;
                value = targetVS[targetIdx].value;
                baseIdx++;
                targetIdx++;
            }
        }

        const auto& it = fvar.find(tag);
        if (it == fvar.end()) {
            continue;  // unsupported axis. Skip.
        }
        const FVarEntry& fvarEntry = it->second;

        if (styleValueUsed && value == fvarEntry.defValue) {
            // Skip the default value if it came from style.
            continue;
        }
        const float clamped = std::clamp(value, fvarEntry.minValue, fvarEntry.maxValue);
        adjustedVars[adjustedHead++] = {tag, clamped};
        if (tag == TAG_wght) {
            // Fake bold is enabled when the max value is more than 200 of difference.
            fakeBold = targetStyle.weight() >= 600 && (targetStyle.weight() - clamped) >= 200;
        }
    }

    // Fake weight is enabled when the TAG_wght is not supported and the weight value has more than
    // 200 of difference.
    if (!hasWght) {
        fakeBold =
                targetStyle.weight() >= 600 && (targetStyle.weight() - baseStyle.weight()) >= 200;
    }
    // Fake italic is enabled when the style is italic and font doesn't support ital or slnt axis.
    bool fakeItalic = false;
    if (targetStyle.isItalic()) {
        if (hasItal || hasSlnt) {
            fakeItalic = false;
        } else {
            fakeItalic = !baseStyle.isItalic();
        }
    }
    return FontFakery(fakeBold, fakeItalic, VariationSettings(adjustedVars, adjustedHead));
}

}  // namespace minikin
