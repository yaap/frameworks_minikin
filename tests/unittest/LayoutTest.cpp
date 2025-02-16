/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <com_android_text_flags.h>
#include <flag_macros.h>
#include <gtest/gtest.h>

#include "FontTestUtils.h"
#include "UnicodeUtils.h"
#include "minikin/FontCollection.h"
#include "minikin/Layout.h"
#include "minikin/LayoutPieces.h"
#include "minikin/Measurement.h"

namespace minikin {

static void expectAdvances(const std::vector<float>& expected, const std::vector<float>& advances) {
    EXPECT_LE(expected.size(), advances.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], advances[i])
                << i << "th element is different. Expected: " << expected[i]
                << ", Actual: " << advances[i];
    }
}

static void getBounds(const U16StringPiece& text, Bidi bidiFlags, const MinikinPaint& paint,
                      MinikinRect* out) {
    getBounds(text, Range(0, text.size()), bidiFlags, paint, StartHyphenEdit::NO_EDIT,
              EndHyphenEdit::NO_EDIT, out);
}

class LayoutTest : public testing::Test {
protected:
    LayoutTest() : mCollection(nullptr) {}

    virtual ~LayoutTest() {}

    virtual void SetUp() override { mCollection = buildFontCollection("Ascii.ttf"); }

    virtual void TearDown() override {}

    std::shared_ptr<FontCollection> mCollection;
};

TEST_F(LayoutTest, doLayoutTest) {
    MinikinPaint paint(mCollection);
    paint.size = 10.0f;  // make 1em = 10px
    MinikinRect rect;
    std::vector<float> expectedValues;

    std::vector<uint16_t> text;

    // The mock implementation returns 10.0f advance and 0,0-10x10 bounds for all glyph.
    {
        SCOPED_TRACE("one word");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(70.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(70.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two words");
        text = utf8ToUtf16("two words");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(90.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(90.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("three words");
        text = utf8ToUtf16("three words test");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(160.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(160.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two spaces");
        text = utf8ToUtf16("two  spaces");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(110.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(110.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
}

TEST_F(LayoutTest, doLayoutTest_wordSpacing) {
    MinikinPaint paint(mCollection);
    paint.size = 10.0f;  // make 1em = 10px
    MinikinRect rect;
    std::vector<float> expectedValues;
    std::vector<uint16_t> text;

    paint.wordSpacing = 5.0f;

    // The mock implementation returns 10.0f advance and 0,0-10x10 bounds for all glyph.
    {
        SCOPED_TRACE("one word");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(70.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(70.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two words");
        text = utf8ToUtf16("two words");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(95.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(95.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[3] = 15.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("three words test");
        text = utf8ToUtf16("three words test");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(170.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(170.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[5] = 15.0f;
        expectedValues[11] = 15.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two spaces");
        text = utf8ToUtf16("two  spaces");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(120.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(120.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[3] = 15.0f;
        expectedValues[4] = 15.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
}

TEST_F(LayoutTest, doLayoutTest_negativeWordSpacing) {
    MinikinPaint paint(mCollection);
    paint.size = 10.0f;  // make 1em = 10px
    MinikinRect rect;
    std::vector<float> expectedValues;

    std::vector<uint16_t> text;

    // Negative word spacing also should work.
    paint.wordSpacing = -5.0f;

    {
        SCOPED_TRACE("one word");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(70.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(70.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two words");
        text = utf8ToUtf16("two words");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(85.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(85.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[3] = 5.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("three words");
        text = utf8ToUtf16("three word test");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(140.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(140.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[5] = 5.0f;
        expectedValues[10] = 5.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
    {
        SCOPED_TRACE("two spaces");
        text = utf8ToUtf16("two  spaces");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(100.0f, layout.getAdvance());

        getBounds(text, Bidi::LTR, paint, &rect);
        EXPECT_EQ(0.0f, rect.mLeft);
        EXPECT_EQ(-10.0f, rect.mTop);
        EXPECT_EQ(100.0f, rect.mRight);
        EXPECT_EQ(0.0f, rect.mBottom);
        expectedValues.resize(text.size());
        for (size_t i = 0; i < expectedValues.size(); ++i) {
            expectedValues[i] = 10.0f;
        }
        expectedValues[3] = 5.0f;
        expectedValues[4] = 5.0f;
        expectAdvances(expectedValues, layout.getAdvances());
    }
}

// Test that a forced-RTL layout correctly mirros a forced-LTR layout.
TEST_F(LayoutTest, doLayoutTest_rtlTest) {
    MinikinPaint paint(mCollection);

    std::vector<uint16_t> text = parseUnicodeString("'a' 'b' U+3042 U+3043 'c' 'd'");
    Range range(0, text.size());

    Layout ltrLayout(text, range, Bidi::FORCE_LTR, paint, StartHyphenEdit::NO_EDIT,
                     EndHyphenEdit::NO_EDIT, RunFlag::NONE);

    Layout rtlLayout(text, range, Bidi::FORCE_RTL, paint, StartHyphenEdit::NO_EDIT,
                     EndHyphenEdit::NO_EDIT, RunFlag::NONE);

    ASSERT_EQ(ltrLayout.nGlyphs(), rtlLayout.nGlyphs());
    ASSERT_EQ(6u, ltrLayout.nGlyphs());

    size_t nGlyphs = ltrLayout.nGlyphs();
    for (size_t i = 0; i < nGlyphs; ++i) {
        EXPECT_EQ(ltrLayout.getFont(i), rtlLayout.getFont(nGlyphs - i - 1));
        EXPECT_EQ(ltrLayout.getGlyphId(i), rtlLayout.getGlyphId(nGlyphs - i - 1));
    }
}

// Test that single-run RTL layouts of LTR-only text is laid out identical to an LTR layout.
TEST_F(LayoutTest, singleRunBidiTest) {
    MinikinPaint paint(mCollection);

    std::vector<uint16_t> text = parseUnicodeString("'1' '2' '3'");
    Range range(0, text.size());

    Layout ltrLayout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                     EndHyphenEdit::NO_EDIT, RunFlag::NONE);

    Layout rtlLayout(text, range, Bidi::RTL, paint, StartHyphenEdit::NO_EDIT,
                     EndHyphenEdit::NO_EDIT, RunFlag::NONE);

    Layout defaultRtlLayout(text, range, Bidi::DEFAULT_RTL, paint, StartHyphenEdit::NO_EDIT,
                            EndHyphenEdit::NO_EDIT, RunFlag::NONE);

    const size_t nGlyphs = ltrLayout.nGlyphs();
    ASSERT_EQ(3u, nGlyphs);

    ASSERT_EQ(nGlyphs, rtlLayout.nGlyphs());
    ASSERT_EQ(nGlyphs, defaultRtlLayout.nGlyphs());

    for (size_t i = 0; i < nGlyphs; ++i) {
        EXPECT_EQ(ltrLayout.getFont(i), rtlLayout.getFont(i));
        EXPECT_EQ(ltrLayout.getGlyphId(i), rtlLayout.getGlyphId(i));
        EXPECT_EQ(ltrLayout.getFont(i), defaultRtlLayout.getFont(i));
        EXPECT_EQ(ltrLayout.getGlyphId(i), defaultRtlLayout.getGlyphId(i));
    }
}

TEST_F(LayoutTest, hyphenationTest) {
    MinikinPaint paint(mCollection);
    paint.size = 10.0f;  // make 1em = 10px
    std::vector<uint16_t> text;

    // The mock implementation returns 10.0f advance for all glyphs.
    {
        SCOPED_TRACE("one word with no hyphen edit");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(70.0f, layout.getAdvance());
    }
    {
        SCOPED_TRACE("one word with hyphen insertion at the end");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::INSERT_HYPHEN, RunFlag::NONE);
        EXPECT_EQ(80.0f, layout.getAdvance());
    }
    {
        SCOPED_TRACE("one word with hyphen replacement at the end");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::REPLACE_WITH_HYPHEN, RunFlag::NONE);
        EXPECT_EQ(70.0f, layout.getAdvance());
    }
    {
        SCOPED_TRACE("one word with hyphen insertion at the start");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::INSERT_HYPHEN,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(80.0f, layout.getAdvance());
    }
    {
        SCOPED_TRACE("one word with hyphen insertion at the both ends");
        text = utf8ToUtf16("oneword");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::INSERT_HYPHEN,
                      EndHyphenEdit::INSERT_HYPHEN, RunFlag::NONE);
        EXPECT_EQ(90.0f, layout.getAdvance());
    }
}

TEST_F(LayoutTest, measuredTextTest) {
    // The test font has following coverage and width.
    // U+0020: 10em
    // U+002E (.): 10em
    // U+0043 (C): 100em
    // U+0049 (I): 1em
    // U+004C (L): 50em
    // U+0056 (V): 5em
    // U+0058 (X): 10em
    // U+005F (_): 0em
    // U+FFFD (invalid surrogate will be replaced to this): 7em
    // U+10331 (\uD800\uDF31): 10em
    auto fc = buildFontCollection("LayoutTestFont.ttf");
    {
        MinikinPaint paint(fc);
        std::vector<uint16_t> text = utf8ToUtf16("I");
        std::vector<float> advances(text.size());
        uint32_t clusterCount = 0;
        Range range(0, text.size());
        EXPECT_EQ(1.0f, Layout::measureText(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                                            EndHyphenEdit::NO_EDIT, advances.data(), nullptr,
                                            &clusterCount, RunFlag::NONE));
        ASSERT_EQ(1u, clusterCount);
        ASSERT_EQ(1u, advances.size());
        EXPECT_EQ(1.0f, advances[0]);
    }
    {
        MinikinPaint paint(fc);
        std::vector<uint16_t> text = utf8ToUtf16("IV");
        std::vector<float> advances(text.size());
        uint32_t clusterCount = 0;
        Range range(0, text.size());
        EXPECT_EQ(6.0f, Layout::measureText(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                                            EndHyphenEdit::NO_EDIT, advances.data(), nullptr,
                                            &clusterCount, RunFlag::NONE));
        ASSERT_EQ(2u, clusterCount);
        ASSERT_EQ(2u, advances.size());
        EXPECT_EQ(1.0f, advances[0]);
        EXPECT_EQ(5.0f, advances[1]);
    }
    {
        MinikinPaint paint(fc);
        std::vector<uint16_t> text = utf8ToUtf16("IVX");
        std::vector<float> advances(text.size());
        uint32_t clusterCount = 0;
        Range range(0, text.size());
        EXPECT_EQ(16.0f,
                  Layout::measureText(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                                      EndHyphenEdit::NO_EDIT, advances.data(), nullptr,
                                      &clusterCount, RunFlag::NONE));
        ASSERT_EQ(3u, clusterCount);
        ASSERT_EQ(3u, advances.size());
        EXPECT_EQ(1.0f, advances[0]);
        EXPECT_EQ(5.0f, advances[1]);
        EXPECT_EQ(10.0f, advances[2]);
    }
}

TEST_F_WITH_FLAGS(LayoutTest, testFontRun,
                  REQUIRES_FLAGS_ENABLED(ACONFIG_FLAG(com::android::text::flags,
                                                      typeface_redesign))) {
    auto latinFamily = buildFontFamily("Ascii.ttf");
    auto jaFamily = buildFontFamily("Hiragana.ttf");
    const std::vector<std::shared_ptr<FontFamily>> families = {latinFamily, jaFamily};
    auto fc = FontCollection::create(families);
    {
        MinikinPaint paint(fc);
        paint.size = 10;
        auto text = utf8ToUtf16("abc");  // (0, 3): Latin letters
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(1ul, layout.getFontRunCount());
        EXPECT_EQ(0ul, layout.getFontRunStart(0));
        EXPECT_EQ(3ul, layout.getFontRunEnd(0));
        EXPECT_EQ("Ascii.ttf", getBasename(layout.getFontRunFont(0).typeface()->GetFontPath()));
    }
    {
        MinikinPaint paint(fc);
        paint.size = 10;
        auto text = utf8ToUtf16("abcあいう");  // (0, 3): Latin letters, (3, 6): Japanese letters.
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(2ul, layout.getFontRunCount());
        EXPECT_EQ(0ul, layout.getFontRunStart(0));
        EXPECT_EQ(3ul, layout.getFontRunEnd(0));
        EXPECT_EQ("Ascii.ttf", getBasename(layout.getFontRunFont(0).typeface()->GetFontPath()));
        EXPECT_EQ(3ul, layout.getFontRunStart(1));
        EXPECT_EQ(6ul, layout.getFontRunEnd(1));
        EXPECT_EQ("Hiragana.ttf", getBasename(layout.getFontRunFont(1).typeface()->GetFontPath()));
    }
    {
        MinikinPaint paint(fc);
        paint.size = 10;
        // (0, 3): Latin letters, (3, 6): Japanese letters, (6, 9): Latin letters.
        auto text = utf8ToUtf16("abcあいうdef");
        Range range(0, text.size());
        Layout layout(text, range, Bidi::LTR, paint, StartHyphenEdit::NO_EDIT,
                      EndHyphenEdit::NO_EDIT, RunFlag::NONE);
        EXPECT_EQ(3ul, layout.getFontRunCount());
        EXPECT_EQ(0ul, layout.getFontRunStart(0));
        EXPECT_EQ(3ul, layout.getFontRunEnd(0));
        EXPECT_EQ("Ascii.ttf", getBasename(layout.getFontRunFont(0).typeface()->GetFontPath()));
        EXPECT_EQ(3ul, layout.getFontRunStart(1));
        EXPECT_EQ(6ul, layout.getFontRunEnd(1));
        EXPECT_EQ("Hiragana.ttf", getBasename(layout.getFontRunFont(1).typeface()->GetFontPath()));
        EXPECT_EQ(6ul, layout.getFontRunStart(2));
        EXPECT_EQ(9ul, layout.getFontRunEnd(2));
        EXPECT_EQ("Ascii.ttf", getBasename(layout.getFontRunFont(2).typeface()->GetFontPath()));
    }
}

}  // namespace minikin
