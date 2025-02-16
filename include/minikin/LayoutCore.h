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

#ifndef MINIKIN_LAYOUT_CORE_H
#define MINIKIN_LAYOUT_CORE_H

#include <gtest/gtest_prod.h>

#include <vector>

#include "minikin/Constants.h"
#include "minikin/FontFamily.h"
#include "minikin/Hyphenator.h"
#include "minikin/MinikinExtent.h"
#include "minikin/MinikinFont.h"
#include "minikin/MinikinRect.h"
#include "minikin/Point.h"
#include "minikin/Range.h"
#include "minikin/U16StringPiece.h"

namespace minikin {

struct MinikinPaint;

// Immutable, recycle-able layout result.
class LayoutPiece {
public:
    LayoutPiece(const U16StringPiece& textBuf, const Range& range, bool isRtl,
                const MinikinPaint& paint, StartHyphenEdit startHyphen, EndHyphenEdit endHyphen);
    ~LayoutPiece();

    // Low level accessors.
    const std::vector<uint8_t>& fontIndices() const { return mFontIndices; }
    const std::vector<uint32_t>& glyphIds() const { return mGlyphIds; }
    const std::vector<Point>& points() const { return mPoints; }
    const std::vector<float>& advances() const { return mAdvances; }
    float advance() const { return mAdvance; }
    const MinikinExtent& extent() const { return mExtent; }
    const std::vector<FakedFont>& fonts() const { return mFonts; }
    uint32_t clusterCount() const { return mClusterCount; }

    // Helper accessors
    uint32_t glyphCount() const { return mGlyphIds.size(); }
    const FakedFont& fontAt(int glyphPos) const { return mFonts[mFontIndices[glyphPos]]; }
    uint32_t glyphIdAt(int glyphPos) const { return mGlyphIds[glyphPos]; }
    const Point& pointAt(int glyphPos) const { return mPoints[glyphPos]; }
    uint16_t clusterAt(int glyphPos) const { return mClusters[glyphPos]; }

    uint32_t getMemoryUsage() const {
        return sizeof(uint8_t) * mFontIndices.size() + sizeof(uint32_t) * mGlyphIds.size() +
               sizeof(Point) * mPoints.size() + sizeof(float) * mAdvances.size() + sizeof(float) +
               sizeof(MinikinRect) + sizeof(MinikinExtent);
    }

    static MinikinRect calculateBounds(const LayoutPiece& layout, const MinikinPaint& paint);

private:
    FRIEND_TEST(LayoutTest, doLayoutWithPrecomputedPiecesTest);

    std::vector<uint8_t> mFontIndices;      // per glyph
    std::vector<uint32_t> mGlyphIds;        // per glyph
    std::vector<Point> mPoints;             // per glyph
    std::vector<uint8_t> mClusters;         // per glyph

    std::vector<float> mAdvances;  // per code units

    float mAdvance;
    MinikinExtent mExtent;
    uint32_t mClusterCount;

    std::vector<FakedFont> mFonts;
};

}  // namespace minikin

#endif  // MINIKIN_LAYOUT_CORE_H
