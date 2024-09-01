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

#ifndef MINIKIN_SORTED_VECTOR_H
#define MINIKIN_SORTED_VECTOR_H

#include "minikin/PackedVector.h"

namespace minikin {

// An immutable packed vector that elements are sorted.
template <typename T, size_t ARRAY_SIZE = 2>
class SortedPackedVector {
public:
    SortedPackedVector() {}
    SortedPackedVector(const T* ptr, uint16_t count, bool sorted = false) : mPacked(ptr, count) {
        if (!sorted) {
            sort();
        }
    }
    SortedPackedVector(const std::vector<T>& vec, bool sorted = false) : mPacked(vec) {
        if (!sorted) {
            sort();
        }
    }
    SortedPackedVector(std::initializer_list<T> init, bool sorted = false) : mPacked(init) {
        if (!sorted) {
            sort();
        }
    }

    SortedPackedVector(const SortedPackedVector& o) = default;
    SortedPackedVector& operator=(const SortedPackedVector& o) = default;
    SortedPackedVector(SortedPackedVector&& o) = default;
    SortedPackedVector& operator=(SortedPackedVector&& o) = default;

    uint16_t size() const { return mPacked.size(); }
    bool empty() const { return size() == 0; }

    const T& operator[](uint16_t i) const { return mPacked[i]; }
    const T* data() const { return mPacked.data(); }

    inline bool operator==(const SortedPackedVector<T>& o) const { return mPacked == o.mPacked; }

    inline bool operator!=(const SortedPackedVector<T>& o) const { return !(*this == o); }

    inline const T* begin() const { return mPacked.begin(); }
    inline const T* end() const { return mPacked.end(); }

private:
    void sort() { std::sort(mPacked.begin(), mPacked.end()); }

    PackedVector<T, ARRAY_SIZE> mPacked;
};

}  // namespace minikin
#endif  // MINIKIN_SORTED_VECTOR_H
