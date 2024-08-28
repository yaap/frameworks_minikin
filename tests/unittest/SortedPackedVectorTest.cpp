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

#include "minikin/SortedPackedVector.h"

namespace minikin {

TEST(SortedPackedVector, construct) {
    {
        auto sorted = SortedPackedVector({1, 2, 3, 4, 5});
        EXPECT_EQ(5, sorted.size());
        EXPECT_EQ(1, sorted[0]);
        EXPECT_EQ(2, sorted[1]);
        EXPECT_EQ(3, sorted[2]);
        EXPECT_EQ(4, sorted[3]);
        EXPECT_EQ(5, sorted[4]);
    }
    {
        auto sorted = SortedPackedVector({1, 2, 3, 4, 5}, true);
        EXPECT_EQ(5, sorted.size());
        EXPECT_EQ(1, sorted[0]);
        EXPECT_EQ(2, sorted[1]);
        EXPECT_EQ(3, sorted[2]);
        EXPECT_EQ(4, sorted[3]);
        EXPECT_EQ(5, sorted[4]);
    }
    {
        auto sorted = SortedPackedVector({2, 1, 4, 3, 5});
        EXPECT_EQ(5, sorted.size());
        EXPECT_EQ(1, sorted[0]);
        EXPECT_EQ(2, sorted[1]);
        EXPECT_EQ(3, sorted[2]);
        EXPECT_EQ(4, sorted[3]);
        EXPECT_EQ(5, sorted[4]);
    }
    {
        std::vector<int> vec = {2, 1, 4, 3, 5};
        auto sorted = SortedPackedVector(vec);
        EXPECT_EQ(5, sorted.size());
        EXPECT_EQ(1, sorted[0]);
        EXPECT_EQ(2, sorted[1]);
        EXPECT_EQ(3, sorted[2]);
        EXPECT_EQ(4, sorted[3]);
        EXPECT_EQ(5, sorted[4]);
    }
    {
        auto sorted = SortedPackedVector({1, 2, 3, 4, 5});
        auto copied = SortedPackedVector(sorted);
        EXPECT_EQ(5, copied.size());
        EXPECT_EQ(1, copied[0]);
        EXPECT_EQ(2, copied[1]);
        EXPECT_EQ(3, copied[2]);
        EXPECT_EQ(4, copied[3]);
        EXPECT_EQ(5, copied[4]);
    }
    {
        auto sorted = SortedPackedVector({1, 2, 3, 4, 5});
        auto moved = SortedPackedVector(std::move(sorted));
        EXPECT_EQ(5, moved.size());
        EXPECT_EQ(1, moved[0]);
        EXPECT_EQ(2, moved[1]);
        EXPECT_EQ(3, moved[2]);
        EXPECT_EQ(4, moved[3]);
        EXPECT_EQ(5, moved[4]);
    }
}

}  // namespace minikin
