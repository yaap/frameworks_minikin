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

#include "minikin/PackedVector.h"

namespace minikin {

struct Data {
    int x, y;
};

TEST(PackedVector, construct) {
    {
        PackedVector<int> packed;
        EXPECT_EQ(0u, packed.size());
        EXPECT_TRUE(packed.empty());
    }
    {
        int data[] = {1, 2, 3, 4, 5};

        PackedVector<int> packed(data, 5);
        EXPECT_EQ(5u, packed.size());
        EXPECT_EQ(1, packed[0]);
        EXPECT_EQ(2, packed[1]);
        EXPECT_EQ(3, packed[2]);
        EXPECT_EQ(4, packed[3]);
        EXPECT_EQ(5, packed[4]);
    }
    {
        int data[] = {1, 2, 3, 4, 5};

        PackedVector<int> packed(data + 2, 2);
        EXPECT_EQ(2u, packed.size());
        EXPECT_EQ(3, packed[0]);
        EXPECT_EQ(4, packed[1]);
    }
    {
        std::vector<int> data = {1, 2, 3, 4, 5};

        PackedVector<int> packed(data);
        EXPECT_EQ(5u, packed.size());
        EXPECT_EQ(1, packed[0]);
        EXPECT_EQ(2, packed[1]);
        EXPECT_EQ(3, packed[2]);
        EXPECT_EQ(4, packed[3]);
        EXPECT_EQ(5, packed[4]);
    }
}

TEST(PackedVector, push_back) {
    PackedVector<int> packed;

    packed.push_back(0);
    EXPECT_EQ(1u, packed.size());
    EXPECT_FALSE(packed.empty());
    EXPECT_EQ(0, packed[0]);
    EXPECT_EQ(0, packed.data()[0]);
    EXPECT_EQ(0, *packed.back());

    packed.push_back(10);
    EXPECT_EQ(2u, packed.size());
    EXPECT_FALSE(packed.empty());
    EXPECT_EQ(10, packed[1]);
    EXPECT_EQ(10, packed.data()[1]);
    EXPECT_EQ(10, *packed.back());
}

TEST(PackedVector, compare) {
    {
        PackedVector<int> left = {1, 2, 3, 4, 5};
        PackedVector<int> right = {1, 2, 3, 4, 5};

        EXPECT_TRUE(left == right);
        EXPECT_FALSE(left != right);
    }
    {
        PackedVector<int> left = {1, 2, 3, 4, 5};
        PackedVector<int> right = {1, 2, 3, 4, 5, 6};

        EXPECT_FALSE(left == right);
        EXPECT_TRUE(left != right);
    }
    {
        PackedVector<int> left = {};
        PackedVector<int> right = {};

        EXPECT_TRUE(left == right);
        EXPECT_FALSE(left != right);
    }
    {
        PackedVector<Data> left = {{0, 1}, {2, 3}};
        PackedVector<Data> right = {{0, 1}, {2, 3}};

        EXPECT_TRUE(left == right);
        EXPECT_FALSE(left != right);
    }
    {
        PackedVector<Data> left = {{0, 1}, {2, 3}};
        PackedVector<Data> right = {{0, 1}};

        EXPECT_FALSE(left == right);
        EXPECT_TRUE(left != right);
    }
}

TEST(PackedVector, reserve) {
    {
        PackedVector<int> packed;
        packed.reserve(100);
        EXPECT_EQ(0u, packed.size());
        EXPECT_EQ(100u, packed.capacity());
        packed.shrink_to_fit();
        EXPECT_EQ(0u, packed.size());
        // The PackedVector has minimum capacity for the space of pointers. So cannot expect it
        // becomes 0.
        EXPECT_NE(100u, packed.capacity());
    }
    {
        PackedVector<int> packed;
        packed.reserve(100);
        for (int i = 0; i < 50; ++i) {
            packed.push_back(i);
        }
        EXPECT_EQ(50u, packed.size());
        EXPECT_EQ(100u, packed.capacity());
        packed.shrink_to_fit();
        EXPECT_EQ(50u, packed.size());
        EXPECT_EQ(50u, packed.capacity());
    }
}

TEST(PackedVector, iterator) {
    {
        PackedVector<int> packed = {0, 1, 2, 3, 4, 5};
        std::vector<int> copied(packed.begin(), packed.end());
        EXPECT_EQ(std::vector<int>({0, 1, 2, 3, 4, 5}), copied);
    }
}

TEST(PackedVector, resize) {
    {
        // Reduction
        PackedVector<int> packed = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        packed.resize(10);
        EXPECT_EQ(10u, packed.size());
        EXPECT_EQ(10u, packed.capacity());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}), packed);

        packed.resize(9);
        EXPECT_EQ(9u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9}), packed);

        packed.resize(8);
        EXPECT_EQ(8u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8}), packed);

        packed.resize(7);
        EXPECT_EQ(7u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7}), packed);

        packed.resize(6);
        EXPECT_EQ(6u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6}), packed);

        packed.resize(5);
        EXPECT_EQ(5u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5}), packed);

        packed.resize(4);
        EXPECT_EQ(4u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4}), packed);

        packed.resize(3);
        EXPECT_EQ(3u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3}), packed);

        packed.resize(2);
        EXPECT_EQ(2u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2}), packed);

        packed.resize(1);
        EXPECT_EQ(1u, packed.size());
        EXPECT_EQ(PackedVector<int>({1}), packed);

        packed.resize(0);
        EXPECT_EQ(0u, packed.size());
        EXPECT_EQ(PackedVector<int>({}), packed);
    }
    {
        // Expansion
        PackedVector<int> packed = {};
        packed.resize(1, 1);
        EXPECT_EQ(1u, packed.size());
        EXPECT_EQ(PackedVector<int>({1}), packed);

        packed.resize(2, 2);
        EXPECT_EQ(2u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2}), packed);

        packed.resize(3, 3);
        EXPECT_EQ(3u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3}), packed);

        packed.resize(4, 4);
        EXPECT_EQ(4u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4}), packed);

        packed.resize(5, 5);
        EXPECT_EQ(5u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5}), packed);

        packed.resize(6, 6);
        EXPECT_EQ(6u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6}), packed);

        packed.resize(7, 7);
        EXPECT_EQ(7u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7}), packed);

        packed.resize(8, 8);
        EXPECT_EQ(8u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8}), packed);

        packed.resize(9, 9);
        EXPECT_EQ(9u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9}), packed);

        packed.resize(10, 10);
        EXPECT_EQ(10u, packed.size());
        EXPECT_EQ(PackedVector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}), packed);
    }
}
}  // namespace minikin
