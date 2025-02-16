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

#define ATRACE_TAG ATRACE_TAG_VIEW

#include "minikin/LayoutCore.h"

#include <hb-icu.h>
#include <hb-ot.h>
#include <log/log.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>
#include <utils/LruCache.h>
#include <utils/Trace.h>

#include <cmath>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "BidiUtils.h"
#include "LayoutUtils.h"
#include "LetterSpacingUtils.h"
#include "LocaleListCache.h"
#include "MinikinInternal.h"
#include "ScriptUtils.h"
#include "minikin/Emoji.h"
#include "minikin/FontFeature.h"
#include "minikin/HbUtils.h"
#include "minikin/LayoutCache.h"
#include "minikin/LayoutPieces.h"
#include "minikin/Macros.h"

namespace minikin {

namespace {

struct SkiaArguments {
    const MinikinFont* font;
    const MinikinPaint* paint;
    FontFakery fakery;
};

// Returns true if the character needs to be excluded for the line spacing.
inline bool isLineSpaceExcludeChar(uint16_t c) {
    return c == CHAR_LINE_FEED || c == CHAR_CARRIAGE_RETURN;
}

static hb_position_t harfbuzzGetGlyphHorizontalAdvance(hb_font_t* /* hbFont */, void* fontData,
                                                       hb_codepoint_t glyph, void* /* userData */) {
    SkiaArguments* args = reinterpret_cast<SkiaArguments*>(fontData);
    float advance = args->font->GetHorizontalAdvance(glyph, *args->paint, args->fakery);
    return 256 * advance + 0.5;
}

static void harfbuzzGetGlyphHorizontalAdvances(hb_font_t* /* hbFont */, void* fontData,
                                               unsigned int count,
                                               const hb_codepoint_t* first_glyph,
                                               unsigned glyph_stride, hb_position_t* first_advance,
                                               unsigned advance_stride, void* /* userData */) {
    SkiaArguments* args = reinterpret_cast<SkiaArguments*>(fontData);
    std::vector<uint16_t> glyphVec(count);
    std::vector<float> advVec(count);

    const hb_codepoint_t* glyph = first_glyph;
    for (uint32_t i = 0; i < count; ++i) {
        glyphVec[i] = *glyph;
        glyph = reinterpret_cast<const hb_codepoint_t*>(reinterpret_cast<const uint8_t*>(glyph) +
                                                        glyph_stride);
    }

    args->font->GetHorizontalAdvances(glyphVec.data(), count, *args->paint, args->fakery,
                                      advVec.data());

    hb_position_t* advances = first_advance;
    for (uint32_t i = 0; i < count; ++i) {
        *advances = HBFloatToFixed(advVec[i]);
        advances = reinterpret_cast<hb_position_t*>(reinterpret_cast<uint8_t*>(advances) +
                                                    advance_stride);
    }
}

static hb_bool_t harfbuzzGetGlyphHorizontalOrigin(hb_font_t* /* hbFont */, void* /* fontData */,
                                                  hb_codepoint_t /* glyph */,
                                                  hb_position_t* /* x */, hb_position_t* /* y */,
                                                  void* /* userData */) {
    // Just return true, following the way that Harfbuzz-FreeType implementation does.
    return true;
}

hb_font_funcs_t* getFontFuncs() {
    static hb_font_funcs_t* fontFuncs = nullptr;
    static std::once_flag once;
    std::call_once(once, [&]() {
        fontFuncs = hb_font_funcs_create();
        // Override the h_advance function since we can't use HarfBuzz's implemenation. It may
        // return the wrong value if the font uses hinting aggressively.
        hb_font_funcs_set_glyph_h_advance_func(fontFuncs, harfbuzzGetGlyphHorizontalAdvance, 0, 0);
        hb_font_funcs_set_glyph_h_advances_func(fontFuncs, harfbuzzGetGlyphHorizontalAdvances, 0,
                                                0);
        hb_font_funcs_set_glyph_h_origin_func(fontFuncs, harfbuzzGetGlyphHorizontalOrigin, 0, 0);
        hb_font_funcs_make_immutable(fontFuncs);
    });
    return fontFuncs;
}

hb_font_funcs_t* getFontFuncsForEmoji() {
    static hb_font_funcs_t* fontFuncs = nullptr;
    static std::once_flag once;
    std::call_once(once, [&]() {
        fontFuncs = hb_font_funcs_create();
        // Don't override the h_advance function since we use HarfBuzz's implementation for emoji
        // for performance reasons.
        // Note that it is technically possible for a TrueType font to have outline and embedded
        // bitmap at the same time. We ignore modified advances of hinted outline glyphs in that
        // case.
        hb_font_funcs_set_glyph_h_origin_func(fontFuncs, harfbuzzGetGlyphHorizontalOrigin, 0, 0);
        hb_font_funcs_make_immutable(fontFuncs);
    });
    return fontFuncs;
}

static bool isColorBitmapFont(const HbFontUniquePtr& font) {
    HbBlob cbdt(font, HB_TAG('C', 'B', 'D', 'T'));
    return cbdt;
}

static inline hb_codepoint_t determineHyphenChar(hb_codepoint_t preferredHyphen, hb_font_t* font) {
    hb_codepoint_t glyph;
    if (preferredHyphen == 0x058A    /* ARMENIAN_HYPHEN */
        || preferredHyphen == 0x05BE /* HEBREW PUNCTUATION MAQAF */
        || preferredHyphen == 0x1400 /* CANADIAN SYLLABIC HYPHEN */) {
        if (hb_font_get_nominal_glyph(font, preferredHyphen, &glyph)) {
            return preferredHyphen;
        } else {
            // The original hyphen requested was not supported. Let's try and see if the
            // Unicode hyphen is supported.
            preferredHyphen = CHAR_HYPHEN;
        }
    }
    if (preferredHyphen == CHAR_HYPHEN) { /* HYPHEN */
        // Fallback to ASCII HYPHEN-MINUS if the font didn't have a glyph for the preferred hyphen.
        // Note that we intentionally don't do anything special if the font doesn't have a
        // HYPHEN-MINUS either, so a tofu could be shown, hinting towards something missing.
        if (!hb_font_get_nominal_glyph(font, preferredHyphen, &glyph)) {
            return 0x002D;  // HYPHEN-MINUS
        }
    }
    return preferredHyphen;
}

template <typename HyphenEdit>
static inline void addHyphenToHbBuffer(const HbBufferUniquePtr& buffer, const HbFontUniquePtr& font,
                                       HyphenEdit hyphen, uint32_t cluster) {
    auto [chars, size] = getHyphenString(hyphen);
    for (size_t i = 0; i < size; i++) {
        hb_buffer_add(buffer.get(), determineHyphenChar(chars[i], font.get()), cluster);
    }
}

// Returns the cluster value assigned to the first codepoint added to the buffer, which can be used
// to translate cluster values returned by HarfBuzz to input indices.
static inline uint32_t addToHbBuffer(const HbBufferUniquePtr& buffer, const uint16_t* buf,
                                     size_t start, size_t count, size_t bufSize,
                                     ssize_t scriptRunStart, ssize_t scriptRunEnd,
                                     StartHyphenEdit inStartHyphen, EndHyphenEdit inEndHyphen,
                                     const HbFontUniquePtr& hbFont) {
    // Only hyphenate the very first script run for starting hyphens.
    const StartHyphenEdit startHyphen =
            (scriptRunStart == 0) ? inStartHyphen : StartHyphenEdit::NO_EDIT;
    // Only hyphenate the very last script run for ending hyphens.
    const EndHyphenEdit endHyphen =
            (static_cast<size_t>(scriptRunEnd) == count) ? inEndHyphen : EndHyphenEdit::NO_EDIT;

    // In the following code, we drop the pre-context and/or post-context if there is a
    // hyphen edit at that end. This is not absolutely necessary, since HarfBuzz uses
    // contexts only for joining scripts at the moment, e.g. to determine if the first or
    // last letter of a text range to shape should take a joining form based on an
    // adjacent letter or joiner (that comes from the context).
    //
    // TODO: Revisit this for:
    // 1. Desperate breaks for joining scripts like Arabic (where it may be better to keep
    //    the context);
    // 2. Special features like start-of-word font features (not implemented in HarfBuzz
    //    yet).

    // We don't have any start-of-line replacement edit yet, so we don't need to check for
    // those.
    if (isInsertion(startHyphen)) {
        // A cluster value of zero guarantees that the inserted hyphen will be in the same
        // cluster with the next codepoint, since there is no pre-context.
        addHyphenToHbBuffer(buffer, hbFont, startHyphen, 0 /* cluster */);
    }

    const uint16_t* hbText;
    int hbTextLength;
    unsigned int hbItemOffset;
    unsigned int hbItemLength = scriptRunEnd - scriptRunStart;  // This is >= 1.

    const bool hasEndInsertion = isInsertion(endHyphen);
    const bool hasEndReplacement = isReplacement(endHyphen);
    if (hasEndReplacement) {
        // Skip the last code unit while copying the buffer for HarfBuzz if it's a replacement. We
        // don't need to worry about non-BMP characters yet since replacements are only done for
        // code units at the moment.
        hbItemLength -= 1;
    }

    if (startHyphen == StartHyphenEdit::NO_EDIT) {
        // No edit at the beginning. Use the whole pre-context.
        hbText = buf;
        hbItemOffset = start + scriptRunStart;
    } else {
        // There's an edit at the beginning. Drop the pre-context and start the buffer at where we
        // want to start shaping.
        hbText = buf + start + scriptRunStart;
        hbItemOffset = 0;
    }

    if (endHyphen == EndHyphenEdit::NO_EDIT) {
        // No edit at the end, use the whole post-context.
        hbTextLength = (buf + bufSize) - hbText;
    } else {
        // There is an edit at the end. Drop the post-context.
        hbTextLength = hbItemOffset + hbItemLength;
    }

    hb_buffer_add_utf16(buffer.get(), hbText, hbTextLength, hbItemOffset, hbItemLength);

    unsigned int numCodepoints;
    hb_glyph_info_t* cpInfo = hb_buffer_get_glyph_infos(buffer.get(), &numCodepoints);

    // Add the hyphen at the end, if there's any.
    if (hasEndInsertion || hasEndReplacement) {
        // When a hyphen is inserted, by assigning the added hyphen and the last
        // codepoint added to the HarfBuzz buffer to the same cluster, we can make sure
        // that they always remain in the same cluster, even if the last codepoint gets
        // merged into another cluster (for example when it's a combining mark).
        //
        // When a replacement happens instead, we want it to get the cluster value of
        // the character it's replacing, which is one "codepoint length" larger than
        // the last cluster. But since the character replaced is always just one
        // code unit, we can just add 1.
        uint32_t hyphenCluster;
        if (numCodepoints == 0) {
            // Nothing was added to the HarfBuzz buffer. This can only happen if
            // we have a replacement that is replacing a one-code unit script run.
            hyphenCluster = 0;
        } else {
            hyphenCluster = cpInfo[numCodepoints - 1].cluster + (uint32_t)hasEndReplacement;
        }
        addHyphenToHbBuffer(buffer, hbFont, endHyphen, hyphenCluster);
        // Since we have just added to the buffer, cpInfo no longer necessarily points to
        // the right place. Refresh it.
        cpInfo = hb_buffer_get_glyph_infos(buffer.get(), nullptr /* we don't need the size */);
    }
    return cpInfo[0].cluster;
}

}  // namespace

LayoutPiece::LayoutPiece(const U16StringPiece& textBuf, const Range& range, bool isRtl,
                         const MinikinPaint& paint, StartHyphenEdit startHyphen,
                         EndHyphenEdit endHyphen) {
    const uint16_t* buf = textBuf.data();
    const size_t start = range.getStart();
    const size_t count = range.getLength();
    const size_t bufSize = textBuf.size();

    mAdvances.resize(count, 0);  // Need zero filling.

    // Usually the number of glyphs are less than number of code units.
    mFontIndices.reserve(count);
    mGlyphIds.reserve(count);
    mPoints.reserve(count);
    mClusters.reserve(count);

    HbBufferUniquePtr buffer(hb_buffer_create());
    U16StringPiece substr = textBuf.substr(range);
    std::vector<FontCollection::Run> items =
            paint.font->itemize(substr, paint.fontStyle, paint.localeListId, paint.familyVariant);

    std::vector<hb_feature_t> features = cleanAndAddDefaultFontFeatures(paint);

    std::vector<HbFontUniquePtr> hbFonts;
    double size = paint.size;
    double scaleX = paint.scaleX;

    std::unordered_map<const MinikinFont*, uint32_t> fontMap;

    float x = 0;
    float y = 0;

    constexpr uint32_t MAX_LENGTH_FOR_BITSET = 256;  // std::bit_ceil(CHAR_LIMIT_FOR_CACHE);
    std::bitset<MAX_LENGTH_FOR_BITSET> clusterSet;
    std::set<uint32_t> clusterSetForLarge;
    const bool useLargeSet = count >= MAX_LENGTH_FOR_BITSET;

    for (int run_ix = isRtl ? items.size() - 1 : 0;
         isRtl ? run_ix >= 0 : run_ix < static_cast<int>(items.size());
         isRtl ? --run_ix : ++run_ix) {
        FontCollection::Run& run = items[run_ix];
        FakedFont fakedFont = paint.font->getBestFont(substr, run, paint.fontStyle);
        const std::shared_ptr<MinikinFont>& typeface = fakedFont.typeface();
        auto it = fontMap.find(typeface.get());
        uint8_t font_ix;
        if (it == fontMap.end()) {
            // First time to see this font.
            font_ix = mFonts.size();
            mFonts.push_back(fakedFont);
            fontMap.insert(std::make_pair(typeface.get(), font_ix));

            // We override some functions which are not thread safe.
            HbFontUniquePtr font(hb_font_create_sub_font(fakedFont.hbFont().get()));
            hb_font_set_funcs(
                    font.get(), isColorBitmapFont(font) ? getFontFuncsForEmoji() : getFontFuncs(),
                    new SkiaArguments({fakedFont.typeface().get(), &paint, fakedFont.fakery}),
                    [](void* data) { delete reinterpret_cast<SkiaArguments*>(data); });
            hbFonts.push_back(std::move(font));
        } else {
            font_ix = it->second;
        }
        const HbFontUniquePtr& hbFont = hbFonts[font_ix];

        bool needExtent = false;
        for (int i = run.start; i < run.end; ++i) {
            if (!isLineSpaceExcludeChar(buf[i])) {
                needExtent = true;
                break;
            }
        }
        if (needExtent) {
            MinikinExtent verticalExtent;
            typeface->GetFontExtent(&verticalExtent, paint, fakedFont.fakery);
            mExtent.extendBy(verticalExtent);
        }

        hb_font_set_ppem(hbFont.get(), size * scaleX, size);
        hb_font_set_scale(hbFont.get(), HBFloatToFixed(size * scaleX), HBFloatToFixed(size));

        // TODO: if there are multiple scripts within a font in an RTL run,
        // we need to reorder those runs. This is unlikely with our current
        // font stack, but should be done for correctness.

        // Note: scriptRunStart and scriptRunEnd, as well as run.start and run.end, run between 0
        // and count.
        for (const auto [range, script] : ScriptText(textBuf, run.start, run.end)) {
            ssize_t scriptRunStart = range.getStart();
            ssize_t scriptRunEnd = range.getEnd();

            // After the last line, scriptRunEnd is guaranteed to have increased, since the only
            // time getScriptRun does not increase its iterator is when it has already reached the
            // end of the buffer. But that can't happen, since if we have already reached the end
            // of the buffer, we should have had (scriptRunEnd == run.end), which means
            // (scriptRunStart == run.end) which is impossible due to the exit condition of the for
            // loop. So we can be sure that scriptRunEnd > scriptRunStart.

            double letterSpace = 0.0;
            double letterSpaceHalf = 0.0;

            if (paint.letterSpacing != 0.0 && isLetterSpacingCapableScript(script)) {
                letterSpace = paint.letterSpacing * size * scaleX;
                letterSpaceHalf = letterSpace * 0.5;
            }

            hb_buffer_clear_contents(buffer.get());
            hb_buffer_set_script(buffer.get(), script);
            hb_buffer_set_direction(buffer.get(), isRtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
            const LocaleList& localeList = LocaleListCache::getById(paint.localeListId);
            if (localeList.size() != 0) {
                hb_language_t hbLanguage = localeList.getHbLanguage(0);
                for (size_t i = 0; i < localeList.size(); ++i) {
                    if (localeList[i].supportsScript(hb_script_to_iso15924_tag(script))) {
                        hbLanguage = localeList.getHbLanguage(i);
                        break;
                    }
                }
                hb_buffer_set_language(buffer.get(), hbLanguage);
            }

            const uint32_t clusterStart =
                    addToHbBuffer(buffer, buf, start, count, bufSize, scriptRunStart, scriptRunEnd,
                                  startHyphen, endHyphen, hbFont);

            hb_shape(hbFont.get(), buffer.get(), features.empty() ? NULL : &features[0],
                     features.size());
            unsigned int numGlyphs;
            hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer.get(), &numGlyphs);
            hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer.get(), NULL);

            // At this point in the code, the cluster values in the info buffer correspond to the
            // input characters with some shift. The cluster value clusterStart corresponds to the
            // first character passed to HarfBuzz, which is at buf[start + scriptRunStart] whose
            // advance needs to be saved into mAdvances[scriptRunStart]. So cluster values need to
            // be reduced by (clusterStart - scriptRunStart) to get converted to indices of
            // mAdvances.
            const ssize_t clusterOffset = clusterStart - scriptRunStart;

            if (numGlyphs && letterSpace != 0) {
                const uint32_t advIndex = info[0].cluster - clusterOffset;
                const uint32_t cp = textBuf.codePointAt(advIndex + start);
                if (!u_iscntrl(cp)) {
                    mAdvances[advIndex] += letterSpaceHalf;
                    x += letterSpaceHalf;
                }
            }
            for (unsigned int i = 0; i < numGlyphs; i++) {
                const size_t clusterBaseIndex = info[i].cluster - clusterOffset;
                if (letterSpace != 0 && i > 0 && info[i - 1].cluster != info[i].cluster) {
                    const uint32_t prevAdvIndex = info[i - 1].cluster - clusterOffset;
                    const uint32_t prevCp = textBuf.codePointAt(prevAdvIndex + start);
                    const uint32_t cp = textBuf.codePointAt(clusterBaseIndex + start);

                    const bool isCtrl = u_iscntrl(cp);
                    const bool isPrevCtrl = u_iscntrl(prevCp);
                    if (!isPrevCtrl) {
                        mAdvances[prevAdvIndex] += letterSpaceHalf;
                    }

                    if (!isCtrl) {
                        mAdvances[clusterBaseIndex] += letterSpaceHalf;
                    }

                    // To avoid rounding error, add full letter spacing when the both prev and
                    // current code point are non-control characters.
                    if (!isCtrl && !isPrevCtrl) {
                        x += letterSpace;
                    } else if (!isCtrl || !isPrevCtrl) {
                        x += letterSpaceHalf;
                    }
                }

                hb_codepoint_t glyph_ix = info[i].codepoint;
                float xoff = HBFixedToFloat(positions[i].x_offset);
                float yoff = -HBFixedToFloat(positions[i].y_offset);
                xoff += yoff * paint.skewX;
                mFontIndices.push_back(font_ix);
                mGlyphIds.push_back(glyph_ix);
                mPoints.emplace_back(x + xoff, y + yoff);
                float xAdvance = HBFixedToFloat(positions[i].x_advance);
                mClusters.push_back(clusterBaseIndex);
                if (useLargeSet) {
                    clusterSetForLarge.insert(clusterBaseIndex);
                } else {
                    clusterSet.set(clusterBaseIndex);
                }

                if (clusterBaseIndex < count) {
                    mAdvances[clusterBaseIndex] += xAdvance;
                } else {
                    ALOGE("cluster %zu (start %zu) out of bounds of count %zu", clusterBaseIndex,
                          start, count);
                }
                x += xAdvance;
            }
            if (numGlyphs && letterSpace != 0) {
                const uint32_t lastAdvIndex = info[numGlyphs - 1].cluster - clusterOffset;
                const uint32_t lastCp = textBuf.codePointAt(lastAdvIndex + start);
                if (!u_iscntrl(lastCp)) {
                    mAdvances[lastAdvIndex] += letterSpaceHalf;
                    x += letterSpaceHalf;
                }
            }
        }
    }
    mFontIndices.shrink_to_fit();
    mGlyphIds.shrink_to_fit();
    mPoints.shrink_to_fit();
    mClusters.shrink_to_fit();
    mAdvance = x;
    if (useLargeSet) {
        mClusterCount = clusterSetForLarge.size();
    } else {
        mClusterCount = clusterSet.count();
    }
}

// static
MinikinRect LayoutPiece::calculateBounds(const LayoutPiece& layout, const MinikinPaint& paint) {
    ATRACE_CALL();
    MinikinRect out;
    for (uint32_t i = 0; i < layout.glyphCount(); ++i) {
        MinikinRect bounds;
        uint32_t glyphId = layout.glyphIdAt(i);
        const FakedFont& fakedFont = layout.fontAt(i);
        const Point& pos = layout.pointAt(i);

        fakedFont.typeface()->GetBounds(&bounds, glyphId, paint, fakedFont.fakery);
        out.join(bounds, pos);
    }
    return out;
}

LayoutPiece::~LayoutPiece() {}

}  // namespace minikin
