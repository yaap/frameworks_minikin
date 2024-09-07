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

#include "FontTestUtils.h"
#include "minikin/Constants.h"
#include "minikin/MinikinPaint.h"

namespace minikin {

TEST(MinikinPaintTest, variationSettings_empty_default) {
    auto fc = buildFontCollection("Ascii.ttf");
    MinikinPaint paint(fc);
    EXPECT_TRUE(paint.fontVariationSettings.empty());
}

TEST(MinikinPaintTest, variationSettings_varsettings_produce_different_hash) {
    auto fc = buildFontCollection("Ascii.ttf");
    MinikinPaint left(fc);
    MinikinPaint right(fc);
    left.fontVariationSettings = VariationSettings({{TAG_wght, 400}, {TAG_ital, 1}});

    EXPECT_NE(left, right);
}

TEST(MinikinPaintTest, variationSettings_different_varsettings) {
    auto fc = buildFontCollection("Ascii.ttf");
    MinikinPaint left(fc);
    MinikinPaint right(fc);
    left.fontVariationSettings = VariationSettings({{TAG_wght, 400}, {TAG_ital, 1}});
    right.fontVariationSettings = VariationSettings({{TAG_wght, 500}, {TAG_ital, 1}});

    EXPECT_NE(left, right);
}

TEST(MinikinPaintTest, variationSettings) {
    auto fc = buildFontCollection("Ascii.ttf");
    MinikinPaint left(fc);
    MinikinPaint right(fc);
    left.fontVariationSettings = VariationSettings({{TAG_wght, 400}, {TAG_ital, 1}});
    right.fontVariationSettings = VariationSettings({{TAG_ital, 1}, {TAG_wght, 400}});
    EXPECT_EQ(left.hash(), right.hash());
    EXPECT_EQ(left, right);
}

}  // namespace minikin
