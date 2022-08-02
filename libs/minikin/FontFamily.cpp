/*
 * Copyright (C) 2013 The Android Open Source Project
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

#define LOG_TAG "Minikin"

#include "minikin/FontFamily.h"

#include <algorithm>
#include <vector>

#include <log/log.h>

#include "minikin/CmapCoverage.h"
#include "minikin/FamilyVariant.h"
#include "minikin/HbUtils.h"
#include "minikin/MinikinFont.h"

#include "FontUtils.h"
#include "Locale.h"
#include "LocaleListCache.h"
#include "MinikinInternal.h"

namespace minikin {

FontFamily::FontFamily(std::vector<std::shared_ptr<Font>>&& fonts)
        : FontFamily(FamilyVariant::DEFAULT, std::move(fonts)) {}

FontFamily::FontFamily(FamilyVariant variant, std::vector<std::shared_ptr<Font>>&& fonts)
        : FontFamily(kEmptyLocaleListId, variant, std::move(fonts), false /* isCustomFallback */) {}

FontFamily::FontFamily(uint32_t localeListId, FamilyVariant variant,
                       std::vector<std::shared_ptr<Font>>&& fonts, bool isCustomFallback)
        : mFonts(std::make_unique<std::shared_ptr<Font>[]>(fonts.size())),
          // computeCoverage may update supported axes and coverages later.
          mSupportedAxes(nullptr),
          mCoverage(),
          mCmapFmt14Coverage(nullptr),
          mLocaleListId(localeListId),
          mFontsCount(static_cast<uint32_t>(fonts.size())),
          mSupportedAxesCount(0),
          mCmapFmt14CoverageCount(0),
          mVariant(variant),
          mIsColorEmoji(LocaleListCache::getById(localeListId).getEmojiStyle() ==
                        EmojiStyle::EMOJI),
          mIsCustomFallback(isCustomFallback) {
    MINIKIN_ASSERT(!fonts.empty(), "FontFamily must contain at least one font.");
    MINIKIN_ASSERT(fonts.size() <= std::numeric_limits<uint32_t>::max(),
                   "Number of fonts must be less than 2^32.");
    for (size_t i = 0; i < mFontsCount; i++) {
        mFonts[i] = std::move(fonts[i]);
    }
    computeCoverage();
}

FontFamily::FontFamily(uint32_t localeListId, FamilyVariant variant,
                       std::unique_ptr<std::shared_ptr<Font>[]>&& fonts, uint32_t fontsCount,
                       std::unique_ptr<AxisTag[]>&& supportedAxes, uint32_t supportedAxesCount,
                       bool isColorEmoji, bool isCustomFallback, SparseBitSet&& coverage,
                       std::unique_ptr<SparseBitSet[]>&& cmapFmt14Coverage,
                       uint16_t cmapFmt14CoverageCount)
        : mFonts(std::move(fonts)),
          mSupportedAxes(std::move(supportedAxes)),
          mCoverage(std::move(coverage)),
          mCmapFmt14Coverage(std::move(cmapFmt14Coverage)),
          mLocaleListId(localeListId),
          mFontsCount(fontsCount),
          mSupportedAxesCount(supportedAxesCount),
          mCmapFmt14CoverageCount(cmapFmt14CoverageCount),
          mVariant(variant),
          mIsColorEmoji(isColorEmoji),
          mIsCustomFallback(isCustomFallback) {}

// static
std::shared_ptr<FontFamily> FontFamily::readFrom(BufferReader* reader) {
    uint32_t localeListId = LocaleListCache::readFrom(reader);
    uint32_t fontsCount = reader->read<uint32_t>();
    auto fonts = std::make_unique<std::shared_ptr<Font>[]>(fontsCount);
    for (size_t i = 0; i < fontsCount; i++) {
        fonts[i] = Font::readFrom(reader, localeListId);
    }
    // FamilyVariant is uint8_t
    static_assert(sizeof(FamilyVariant) == 1);
    FamilyVariant variant = reader->read<FamilyVariant>();
    // AxisTag is uint32_t
    static_assert(sizeof(AxisTag) == 4);
    const auto& [axesPtr, axesCount] = reader->readArray<AxisTag>();
    std::unique_ptr<AxisTag[]> supportedAxes = nullptr;
    if (axesCount > 0) {
        supportedAxes = std::unique_ptr<AxisTag[]>(new AxisTag[axesCount]);
        std::copy(axesPtr, axesPtr + axesCount, supportedAxes.get());
    }
    bool isColorEmoji = static_cast<bool>(reader->read<uint8_t>());
    bool isCustomFallback = static_cast<bool>(reader->read<uint8_t>());
    SparseBitSet coverage(reader);
    std::unique_ptr<SparseBitSet[]> cmapFmt14Coverage = nullptr;
    // Read mCmapFmt14Coverage. As it can have null entries, it is stored in the buffer as a sparse
    // array (size, non-null entry count, array of (index, entry)).
    uint32_t cmapFmt14CoverageSize = reader->read<uint32_t>();
    if (cmapFmt14CoverageSize > 0) {
        cmapFmt14Coverage = std::make_unique<SparseBitSet[]>(cmapFmt14CoverageSize);
        uint32_t cmapFmt14CoverageEntryCount = reader->read<uint32_t>();
        for (uint32_t i = 0; i < cmapFmt14CoverageEntryCount; i++) {
            uint32_t index = reader->read<uint32_t>();
            cmapFmt14Coverage[index] = SparseBitSet(reader);
        }
    }
    return std::shared_ptr<FontFamily>(new FontFamily(
            localeListId, variant, std::move(fonts), fontsCount, std::move(supportedAxes),
            axesCount, isColorEmoji, isCustomFallback, std::move(coverage),
            std::move(cmapFmt14Coverage), cmapFmt14CoverageSize));
}

void FontFamily::writeTo(BufferWriter* writer) const {
    LocaleListCache::writeTo(writer, mLocaleListId);
    writer->write<uint32_t>(mFontsCount);
    for (size_t i = 0; i < mFontsCount; i++) {
        mFonts[i]->writeTo(writer);
    }
    writer->write<FamilyVariant>(mVariant);
    writer->writeArray<AxisTag>(mSupportedAxes.get(), mSupportedAxesCount);
    writer->write<uint8_t>(mIsColorEmoji);
    writer->write<uint8_t>(mIsCustomFallback);
    mCoverage.writeTo(writer);
    // Write mCmapFmt14Coverage as a sparse array (size, non-null entry count,
    // array of (index, entry))
    writer->write<uint32_t>(mCmapFmt14CoverageCount);
    // Skip writing the sparse entries if the size is zero
    if (mCmapFmt14CoverageCount > 0) {
        uint32_t cmapFmt14CoverageEntryCount = 0;
        for (size_t i = 0; i < mCmapFmt14CoverageCount; i++) {
            if (!mCmapFmt14Coverage[i].empty()) cmapFmt14CoverageEntryCount++;
        }
        writer->write<uint32_t>(cmapFmt14CoverageEntryCount);
        for (size_t i = 0; i < mCmapFmt14CoverageCount; i++) {
            if (!mCmapFmt14Coverage[i].empty()) {
                writer->write<uint32_t>(i);
                mCmapFmt14Coverage[i].writeTo(writer);
            }
        }
    }
}

// Compute a matching metric between two styles - 0 is an exact match
static int computeMatch(FontStyle style1, FontStyle style2) {
    if (style1 == style2) return 0;
    int score = abs(style1.weight() / 100 - style2.weight() / 100);
    if (style1.slant() != style2.slant()) {
        score += 2;
    }
    return score;
}

static FontFakery computeFakery(FontStyle wanted, FontStyle actual) {
    // If desired weight is semibold or darker, and 2 or more grades
    // higher than actual (for example, medium 500 -> bold 700), then
    // select fake bold.
    bool isFakeBold = wanted.weight() >= 600 && (wanted.weight() - actual.weight()) >= 200;
    bool isFakeItalic = wanted.slant() == FontStyle::Slant::ITALIC &&
                        actual.slant() == FontStyle::Slant::UPRIGHT;
    return FontFakery(isFakeBold, isFakeItalic);
}

FakedFont FontFamily::getClosestMatch(FontStyle style) const {
    int bestIndex = 0;
    Font* bestFont = mFonts[bestIndex].get();
    int bestMatch = computeMatch(bestFont->style(), style);
    for (size_t i = 1; i < mFontsCount; i++) {
        Font* font = mFonts[i].get();
        int match = computeMatch(font->style(), style);
        if (i == 0 || match < bestMatch) {
            bestFont = font;
            bestIndex = i;
            bestMatch = match;
        }
    }
    return FakedFont{mFonts[bestIndex], computeFakery(style, bestFont->style())};
}

void FontFamily::computeCoverage() {
    const std::shared_ptr<Font>& font = getClosestMatch(FontStyle()).font;
    HbBlob cmapTable(font->baseFont(), MinikinFont::MakeTag('c', 'm', 'a', 'p'));
    if (cmapTable.get() == nullptr) {
        ALOGE("Could not get cmap table size!\n");
        return;
    }

    std::vector<SparseBitSet> cmapFmt14Coverage;
    mCoverage = CmapCoverage::getCoverage(cmapTable.get(), cmapTable.size(), &cmapFmt14Coverage);
    static_assert(INVALID_VS_INDEX <= std::numeric_limits<uint16_t>::max());
    // cmapFmt14Coverage maps VS index to coverage.
    // cmapFmt14Coverage's size cannot exceed INVALID_VS_INDEX.
    MINIKIN_ASSERT(cmapFmt14Coverage.size() <= INVALID_VS_INDEX,
                   "cmapFmt14Coverage's size must not exceed INVALID_VS_INDEX.");
    mCmapFmt14CoverageCount = static_cast<uint16_t>(cmapFmt14Coverage.size());
    if (mCmapFmt14CoverageCount > 0) {
        mCmapFmt14Coverage = std::make_unique<SparseBitSet[]>(mCmapFmt14CoverageCount);
        for (size_t i = 0; i < mCmapFmt14CoverageCount; i++) {
            mCmapFmt14Coverage[i] = std::move(cmapFmt14Coverage[i]);
        }
    }

    std::unordered_set<AxisTag> supportedAxesSet;
    for (size_t i = 0; i < mFontsCount; ++i) {
        std::unordered_set<AxisTag> supportedAxes = mFonts[i]->getSupportedAxes();
        supportedAxesSet.insert(supportedAxes.begin(), supportedAxes.end());
    }
    MINIKIN_ASSERT(supportedAxesSet.size() <= std::numeric_limits<uint32_t>::max(),
                   "Number of supported axes must be less than 2^16.");
    mSupportedAxesCount = static_cast<uint16_t>(supportedAxesSet.size());
    if (mSupportedAxesCount > 0) {
        mSupportedAxes = std::unique_ptr<AxisTag[]>(new AxisTag[mSupportedAxesCount]);
        std::copy(supportedAxesSet.begin(), supportedAxesSet.end(), mSupportedAxes.get());
        std::sort(mSupportedAxes.get(), mSupportedAxes.get() + mSupportedAxesCount);
    }
}

bool FontFamily::hasGlyph(uint32_t codepoint, uint32_t variationSelector) const {
    if (variationSelector == 0) {
        return mCoverage.get(codepoint);
    }

    if (mCmapFmt14CoverageCount == 0) {
        return false;
    }

    const uint16_t vsIndex = getVsIndex(variationSelector);

    if (vsIndex >= mCmapFmt14CoverageCount) {
        // Even if vsIndex is INVALID_VS_INDEX, we reach here since INVALID_VS_INDEX is defined to
        // be at the maximum end of the range.
        return false;
    }

    const SparseBitSet& bitset = mCmapFmt14Coverage[vsIndex];
    if (bitset.empty()) {
        return false;
    }

    return bitset.get(codepoint);
}

std::shared_ptr<FontFamily> FontFamily::createFamilyWithVariation(
        const std::vector<FontVariation>& variations) const {
    if (variations.empty() || mSupportedAxesCount == 0) {
        return nullptr;
    }

    bool hasSupportedAxis = false;
    for (const FontVariation& variation : variations) {
        if (std::binary_search(mSupportedAxes.get(), mSupportedAxes.get() + mSupportedAxesCount,
                               variation.axisTag)) {
            hasSupportedAxis = true;
            break;
        }
    }
    if (!hasSupportedAxis) {
        // None of variation axes are suppored by this family.
        return nullptr;
    }

    std::vector<std::shared_ptr<Font>> fonts;
    for (size_t i = 0; i < mFontsCount; i++) {
        const std::shared_ptr<Font>& font = mFonts[i];
        bool supportedVariations = false;
        std::unordered_set<AxisTag> supportedAxes = font->getSupportedAxes();
        if (!supportedAxes.empty()) {
            for (const FontVariation& variation : variations) {
                if (supportedAxes.find(variation.axisTag) != supportedAxes.end()) {
                    supportedVariations = true;
                    break;
                }
            }
        }
        std::shared_ptr<MinikinFont> minikinFont;
        if (supportedVariations) {
            minikinFont = font->typeface()->createFontWithVariation(variations);
        }
        if (minikinFont == nullptr) {
            fonts.push_back(font);
        } else {
            fonts.push_back(Font::Builder(minikinFont).setStyle(font->style()).build());
        }
    }

    return std::shared_ptr<FontFamily>(
            new FontFamily(mLocaleListId, mVariant, std::move(fonts), mIsCustomFallback));
}

}  // namespace minikin
