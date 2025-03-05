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

#ifndef MINIKIN_FONT_H
#define MINIKIN_FONT_H

#include <gtest/gtest_prod.h>
#include <utils/LruCache.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "minikin/Buffer.h"
#include "minikin/FVarTable.h"
#include "minikin/FontFakery.h"
#include "minikin/FontStyle.h"
#include "minikin/FontVariation.h"
#include "minikin/HbUtils.h"
#include "minikin/LocaleList.h"
#include "minikin/Macros.h"
#include "minikin/MinikinFont.h"

namespace minikin {

// Represents a single font file.
class Font {
public:
    class Builder {
    public:
        Builder(const std::shared_ptr<MinikinFont>& typeface) : mTypeface(typeface) {}

        // Override the font style. If not called, info from OS/2 table is used.
        Builder& setStyle(FontStyle style) {
            mWeight = style.weight();
            mSlant = style.slant();
            mIsWeightSet = mIsSlantSet = true;
            return *this;
        }

        // Override the font weight. If not called, info from OS/2 table is used.
        Builder& setWeight(uint16_t weight) {
            mWeight = weight;
            mIsWeightSet = true;
            return *this;
        }

        // Override the font slant. If not called, info from OS/2 table is used.
        Builder& setSlant(FontStyle::Slant slant) {
            mSlant = slant;
            mIsSlantSet = true;
            return *this;
        }

        Builder& setLocaleListId(uint32_t id) {
            mLocaleListId = id;
            return *this;
        }

        std::shared_ptr<Font> build();

    private:
        std::shared_ptr<MinikinFont> mTypeface;
        uint16_t mWeight = static_cast<uint16_t>(FontStyle::Weight::NORMAL);
        FontStyle::Slant mSlant = FontStyle::Slant::UPRIGHT;
        uint32_t mLocaleListId = kEmptyLocaleListId;
        bool mIsWeightSet = false;
        bool mIsSlantSet = false;
    };

    explicit Font(BufferReader* reader);
    void writeTo(BufferWriter* writer) const;

    // Create font instance with axes override.
    Font(const std::shared_ptr<Font>& parent, const VariationSettings& axes);

    Font(Font&& o) noexcept;
    Font& operator=(Font&& o) noexcept;
    ~Font();
    // This locale list is just for API compatibility. This is not used in font selection or family
    // fallback.
    uint32_t getLocaleListId() const { return mLocaleListId; }
    inline FontStyle style() const { return mStyle; }

    const HbFontUniquePtr& baseFont() const;
    const std::shared_ptr<MinikinFont>& baseTypeface() const;

    // Returns an adjusted hb_font_t instance and MinikinFont instance.
    // Passing -1 each means do not override the current variation settings.
    HbFontUniquePtr getAdjustedFont(int wght, int ital) const;
    std::shared_ptr<MinikinFont> getAdjustedTypeface(int wght, int ital) const;

    HbFontUniquePtr getAdjustedFont(const VariationSettings& varSettings) const;
    std::shared_ptr<MinikinFont> getAdjustedTypeface(const VariationSettings& varSettings) const;

    BufferReader typefaceMetadataReader() const { return mTypefaceMetadataReader; }

    uint16_t getSupportedAxesCount() const { return mSupportedAxesCount; }
    const AxisTag* getSupportedAxes() const { return mSupportedAxes.get(); }
    bool isAxisSupported(uint32_t tag) const;

    const FVarTable& getFVarTable() const;

private:
    // ExternalRefs holds references to objects provided by external libraries.
    // Because creating these external objects is costly,
    // ExternalRefs is lazily created if Font was created by readFrom().
    class ExternalRefs {
    public:
        ExternalRefs(std::shared_ptr<MinikinFont>&& typeface, HbFontUniquePtr&& baseFont)
                : mTypeface(std::move(typeface)),
                  mBaseFont(std::move(baseFont)),
                  mVarTypefaceCache2(16),
                  mVarFontCache2(16) {}

        std::shared_ptr<MinikinFont> mTypeface;
        HbFontUniquePtr mBaseFont;

        // TODO: remove wght/ital only adjusted typeface pool once redesign typeface flag
        //       is removed.
        const std::shared_ptr<MinikinFont>& getAdjustedTypeface(int wght, int ital) const;
        HbFontUniquePtr getAdjustedFont(int wght, int ital) const;
        mutable std::mutex mMutex;
        mutable std::map<uint16_t, std::shared_ptr<MinikinFont>> mVarTypefaceCache
                GUARDED_BY(mMutex);
        mutable std::map<uint16_t, HbFontUniquePtr> mVarFontCache GUARDED_BY(mMutex);

        std::shared_ptr<MinikinFont> getAdjustedTypeface(const VariationSettings& varSettings,
                                                         const FVarTable& fvarTable) const;
        HbFontUniquePtr getAdjustedFont(const VariationSettings& varSettings,
                                        const FVarTable& fvarTable) const;
        mutable android::LruCache<VariationSettings, std::shared_ptr<MinikinFont>>
                mVarTypefaceCache2 GUARDED_BY(mMutex);
        mutable android::LruCache<VariationSettings, HbFontUniquePtr*> mVarFontCache2
                GUARDED_BY(mMutex);
    };

    // Use Builder instead.
    Font(std::shared_ptr<MinikinFont>&& typeface, FontStyle style, HbFontUniquePtr&& baseFont,
         uint32_t localeListId)
            : mExternalRefsHolder(new ExternalRefs(std::move(typeface), std::move(baseFont))),
              mExternalRefsBuilder(nullptr),
              mStyle(style),
              mLocaleListId(localeListId),
              mTypefaceMetadataReader(nullptr) {
        calculateSupportedAxes();
    }

    void resetExternalRefs(ExternalRefs* refs);

    const ExternalRefs* getExternalRefs() const;
    std::vector<FontVariation> getAdjustedVariations(int wght, int ital) const;

    static HbFontUniquePtr prepareFont(const std::shared_ptr<MinikinFont>& typeface);
    static FontStyle analyzeStyle(const HbFontUniquePtr& font);

    // Lazy-initialized if created by readFrom().
    mutable std::atomic<ExternalRefs*> mExternalRefsHolder;
    std::function<ExternalRefs*()> mExternalRefsBuilder;
    FontStyle mStyle;
    uint32_t mLocaleListId;
    std::unique_ptr<AxisTag[]> mSupportedAxes;
    uint16_t mSupportedAxesCount;

    void calculateSupportedAxes();

    mutable std::atomic<FVarTable*> mFVarTableHolder;

    // Non-null if created by readFrom().
    BufferReader mTypefaceMetadataReader;

    // Stop copying.
    Font(const Font& o) = delete;
    Font& operator=(const Font& o) = delete;

    FRIEND_TEST(FontTest, MoveConstructorTest);
    FRIEND_TEST(FontTest, MoveAssignmentTest);
};

struct FakedFont {
    inline bool operator==(const FakedFont& o) const {
        return font == o.font && fakery == o.fakery;
    }
    inline bool operator!=(const FakedFont& o) const { return !(*this == o); }

    HbFontUniquePtr hbFont() const;
    std::shared_ptr<MinikinFont> typeface() const;

    // ownership is the enclosing FontCollection
    // FakedFont will be stored in the LayoutCache. It is not a good idea too keep font instance
    // even if the enclosing FontCollection, i.e. Typeface is GC-ed. The layout cache is only
    // purged when it is overflown, thus intentionally keep only reference.
    const std::shared_ptr<Font>& font;
    FontFakery fakery;
};

}  // namespace minikin

#endif  // MINIKIN_FONT_H
