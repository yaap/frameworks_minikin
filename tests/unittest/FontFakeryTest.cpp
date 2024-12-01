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

#include <gtest/gtest.h>

#include "FontVariationTestUtils.h"
#include "minikin/Constants.h"
#include "minikin/FontFakery.h"

namespace minikin {

namespace {

constexpr FontStyle THIN = FontStyle(FontStyle::Weight::THIN, FontStyle::Slant::UPRIGHT);
constexpr FontStyle REGULAR = FontStyle(FontStyle::Weight::NORMAL, FontStyle::Slant::UPRIGHT);
constexpr FontStyle MEDIUM = FontStyle(FontStyle::Weight::MEDIUM, FontStyle::Slant::UPRIGHT);
constexpr FontStyle BOLD = FontStyle(FontStyle::Weight::BOLD, FontStyle::Slant::UPRIGHT);
constexpr FontStyle BLACK = FontStyle(FontStyle::Weight::BLACK, FontStyle::Slant::UPRIGHT);
constexpr FontStyle ITALIC = FontStyle(FontStyle::Weight::NORMAL, FontStyle::Slant::ITALIC);
constexpr FontStyle BOLD_ITALIC = FontStyle(FontStyle::Weight::BOLD, FontStyle::Slant::ITALIC);

FontFakery merge(const FVarTable& fvar, const std::string& base, const std::string& target,
                 FontStyle baseStyle, FontStyle targetStyle) {
    return merge(fvar, parseVariationSettings(base), parseVariationSettings(target), baseStyle,
                 targetStyle);
}

}  // namespace

inline bool operator==(const char* expect, const VariationSettings& vs) {
    VariationSettings expectVarSettings = parseVariationSettings(expect);
    return expectVarSettings == vs;
}

TEST(FontFakeryTest, testConstruct) {
    EXPECT_EQ(FontFakery(), FontFakery(false, false));
    EXPECT_NE(FontFakery(), FontFakery(true, false));
    EXPECT_NE(FontFakery(), FontFakery(false, true));
    EXPECT_NE(FontFakery(), FontFakery(true, true));

    EXPECT_TRUE(FontFakery(true, true).isFakeBold());
    EXPECT_TRUE(FontFakery(true, true).isFakeItalic());
    EXPECT_TRUE(FontFakery(true, true).variationSettings().empty());

    EXPECT_FALSE(FontFakery(false, false).isFakeBold());
    EXPECT_FALSE(FontFakery(false, false).isFakeItalic());
    EXPECT_TRUE(FontFakery(false, false).variationSettings().empty());

    EXPECT_TRUE(FontFakery(true, false).isFakeBold());
    EXPECT_FALSE(FontFakery(true, false).isFakeItalic());
    EXPECT_TRUE(FontFakery(true, false).variationSettings().empty());

    EXPECT_FALSE(FontFakery(false, true).isFakeBold());
    EXPECT_TRUE(FontFakery(false, true).isFakeItalic());
    EXPECT_TRUE(FontFakery(false, true).variationSettings().empty());
}

TEST(FontFakeryTest, testVariationSettings) {
    VariationSettings variationSettings = {FontVariation(TAG_wght, 400),
                                           FontVariation(TAG_ital, 1)};

    auto ff = FontFakery(false, false, std::move(variationSettings));

    EXPECT_EQ(2u, ff.variationSettings().size());
    EXPECT_EQ(TAG_ital, ff.variationSettings()[0].axisTag);
    EXPECT_EQ(1, ff.variationSettings()[0].value);
    EXPECT_EQ(TAG_wght, ff.variationSettings()[1].axisTag);
    EXPECT_EQ(400, ff.variationSettings()[1].value);
}

TEST(FontFakeryTest, testMerge) {
    FVarTable fvar = {{MakeTag('A', 'B', 'C', 'D'), {0, 100, 50}}};

    // Override should be used.
    EXPECT_EQ("'ABCD' 100", merge(fvar, "", "'ABCD' 100", REGULAR, REGULAR).variationSettings());
    // Base should be remains. Empty variation settings means use base font without override.
    EXPECT_EQ("", merge(fvar, "'ABCD' 0", "", REGULAR, REGULAR).variationSettings());
    // The default value from the target VS should be preserved.
    EXPECT_EQ("'ABCD' 50", merge(fvar, "", "'ABCD' 50", REGULAR, REGULAR).variationSettings());
    // Override should override the base settings.
    EXPECT_EQ("'ABCD' 100",
              merge(fvar, "'ABCD' 0", "'ABCD' 100", REGULAR, REGULAR).variationSettings());
}

TEST(FontFakeryTest, testMerge_twoAxes) {
    FVarTable fvar = {{MakeTag('A', 'B', 'C', 'D'), {0, 100, 50}},
                      {MakeTag('E', 'F', 'G', 'H'), {0, 100, 50}}};

    // Different axes should be preserved.
    EXPECT_EQ("'ABCD' 100, 'EFGH' 100",
              merge(fvar, "'ABCD' 100", "'EFGH' 100", REGULAR, REGULAR).variationSettings());
    // Overrides override only matched axis.
    EXPECT_EQ(
            "'ABCD' 0, 'EFGH' 100",
            merge(fvar, "'ABCD' 0, 'EFGH' 0", "'EFGH' 100", REGULAR, REGULAR).variationSettings());
}

TEST(FontFakeryTest, testMerge_styleWeight) {
    FVarTable fvar = {{TAG_wght, {100, 900, 400}}, {TAG_ital, {0, 1, 0}}};

    // Default FontStyle sets wght 400 and it is dropped.
    EXPECT_EQ("", merge(fvar, "", "", REGULAR, REGULAR).variationSettings());
    // Use weight of FontStyle if no override is specified.
    EXPECT_EQ("'wght' 100", merge(fvar, "", "", REGULAR, THIN).variationSettings());
    // If override is spseicied, it is used instead of FontStyle.
    EXPECT_EQ("'wght' 500", merge(fvar, "", "'wght' 500", REGULAR, THIN).variationSettings());
}

TEST(FontFakeryTest, testMerge_styleItal) {
    FVarTable fvar = {{TAG_wght, {100, 900, 400}}, {TAG_ital, {0, 1, 0}}};

    // Use weight of FontStyle if no override is specified.
    EXPECT_EQ("'ital' 1", merge(fvar, "", "", REGULAR, ITALIC).variationSettings());
    // Base should be remains. Empty variation settings means use base font without override.
    EXPECT_EQ("", merge(fvar, "'ital' 1", "", REGULAR, REGULAR).variationSettings());
    EXPECT_EQ("'ital' 0", merge(fvar, "'ital' 0", "", REGULAR, ITALIC).variationSettings());
    // If override is spseicied, it is used instead of FontStyle.
    EXPECT_EQ("'ital' 0", merge(fvar, "", "'ital' 0", REGULAR, ITALIC).variationSettings());
}

TEST(FontFakeryTest, testMerge_styleSlnt) {
    FVarTable fvar = {{TAG_wght, {100, 900, 400}}, {TAG_slnt, {-10, 0, 0}}};

    // Use weight of FontStyle if no override is specified.
    EXPECT_EQ("'slnt' -10", merge(fvar, "", "", REGULAR, ITALIC).variationSettings());
    // If override is spseicied, it is used instead of FontStyle.
    EXPECT_EQ("'slnt' 0", merge(fvar, "", "'slnt' 0", REGULAR, ITALIC).variationSettings());
}

TEST(FontFakeryTest, testMerge_complex) {
    FVarTable fvar = {
            {TAG_wght, {100, 900, 400}},
            {TAG_slnt, {-10, 0, 0}},
            {MakeTag('A', 'B', 'C', 'D'), {0, 100, 50}},
    };

    EXPECT_EQ("'wght' 750, 'slnt' -10, 'ABCD' 75",
              merge(fvar, "'wght' 650", "'wght' 750, 'ABCD' 75", REGULAR, ITALIC)
                      .variationSettings());
}

TEST(FontFakeryTest, testMerge_fakeBold_unsupported_font) {
    FVarTable fvar = {};

    // The same weight won't enable fake bold.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, REGULAR).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", BOLD, BOLD).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", THIN, THIN).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", BLACK, BLACK).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, ITALIC).isFakeBold());

    // If the weight diff is more than 200, fake bold is enabled.
    EXPECT_TRUE(merge(fvar, "", "", REGULAR, BOLD).isFakeBold());
    EXPECT_TRUE(merge(fvar, "", "", REGULAR, BLACK).isFakeBold());
    EXPECT_TRUE(merge(fvar, "", "", BOLD, BLACK).isFakeBold());

    // If the requested weight is less than 600, the fake bold is not enabled.
    EXPECT_FALSE(merge(fvar, "", "", THIN, REGULAR).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", THIN, MEDIUM).isFakeBold());
}

TEST(FontFakeryTest, testMerge_fakeBold_fullrange_font) {
    FVarTable fvar = {{TAG_wght, {100, 900, 400}}};

    // If the given font supports full range of weight, the fake bold is never enabled.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, THIN).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, REGULAR).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, MEDIUM).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BOLD).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BLACK).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, ITALIC).isFakeBold());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BOLD_ITALIC).isFakeBold());
}

TEST(FontFakeryTest, testMerge_fakeBold_limited_range_font) {
    FVarTable fvar = {{TAG_wght, {100, 700, 400}}};

    // If the weight diff from the upper limit of the weight is more than 200, fake bold is enabled.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BOLD).isFakeBold());
    EXPECT_TRUE(merge(fvar, "", "", REGULAR, BLACK).isFakeBold());
}

TEST(FontFakeryTest, testMerge_fakeItalic_unsupported_font) {
    FVarTable fvar = {};

    // The same italic won't enable fake italic.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, REGULAR).isFakeItalic());
    EXPECT_FALSE(merge(fvar, "", "", ITALIC, ITALIC).isFakeItalic());
    EXPECT_FALSE(merge(fvar, "", "", BOLD_ITALIC, BOLD_ITALIC).isFakeItalic());

    // If the target style is italic but base style is not, fake bold is enabled.
    EXPECT_TRUE(merge(fvar, "", "", REGULAR, ITALIC).isFakeItalic());
    EXPECT_TRUE(merge(fvar, "", "", REGULAR, BOLD_ITALIC).isFakeItalic());
}

TEST(FontFakeryTest, testMerge_fakeItalic_ital_font) {
    FVarTable fvar = {{TAG_ital, {0, 1, 0}}};

    // If the font supports ital tag, the fake italic is never enabled.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, ITALIC).isFakeItalic());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BOLD_ITALIC).isFakeItalic());
}

TEST(FontFakeryTest, testMerge_fakeItalic_slnt_font) {
    FVarTable fvar = {{TAG_slnt, {-10, 0, 0}}};

    // If the font supports slnt tag, the fake italic is never enabled.
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, ITALIC).isFakeItalic());
    EXPECT_FALSE(merge(fvar, "", "", REGULAR, BOLD_ITALIC).isFakeItalic());
}

}  // namespace minikin
