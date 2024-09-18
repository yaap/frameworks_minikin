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

#include "minikin/Constants.h"
#include "minikin/FontFakery.h"

namespace minikin {

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

}  // namespace minikin
