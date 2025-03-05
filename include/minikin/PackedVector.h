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

#ifndef MINIKIN_PACKED_VECTOR_H
#define MINIKIN_PACKED_VECTOR_H

#include <log/log.h>

#include <type_traits>
#include <vector>

namespace minikin {

// PackedVector optimize short term allocations for small size objects.
// The public interfaces are following the std::vector.
template <typename T, size_t ARRAY_SIZE = 2, typename SIZE_TYPE = uint32_t>
class PackedVector {
private:
    // At least two elements of pointer array is reserved.
    static constexpr size_t PTR_ARRAY_SIZE =
            std::max(static_cast<size_t>(2),
                     (ARRAY_SIZE * sizeof(T) + sizeof(uintptr_t) - 1) / sizeof(uintptr_t));
    // Number of elements can be stored into array.
    static constexpr size_t ARRAY_CAPACITY = PTR_ARRAY_SIZE * sizeof(uintptr_t) / sizeof(T);
    static_assert(std::is_pod<T>::value, "only POD can be stored in PackedVector.");

public:
    typedef T value_type;

    // Constructors
    PackedVector() : mSize(0), mCapacity(ARRAY_CAPACITY) {}
    PackedVector(const T* ptr, SIZE_TYPE size) : PackedVector() { copy(ptr, size); }
    PackedVector(const std::vector<T>& src) : PackedVector() {
        LOG_ALWAYS_FATAL_IF(src.size() >= std::numeric_limits<SIZE_TYPE>::max());
        copy(src.data(), src.size());
    }
    PackedVector(std::initializer_list<T> init) : PackedVector() {
        copy(init.begin(), init.size());
    }

    // Assignments
    PackedVector(const PackedVector& o) : PackedVector() { copy(o.getPtr(), o.mSize); }
    PackedVector& operator=(const PackedVector& o) {
        copy(o.getPtr(), o.mSize);
        return *this;
    }

    // Movement
    PackedVector(PackedVector&& o) : PackedVector() { move(std::move(o)); }
    PackedVector& operator=(PackedVector&& o) {
        move(std::move(o));
        return *this;
    }

    ~PackedVector() { free(); }

    // Compare
    inline bool operator==(const PackedVector& o) const {
        return mSize == o.mSize && memcmp(getPtr(), o.getPtr(), mSize * sizeof(T)) == 0;
    }
    inline bool operator!=(const PackedVector& o) const { return !(*this == o); }

    const T* data() const { return getPtr(); }
    T* data() { return getPtr(); }

    const T& operator[](SIZE_TYPE i) const { return getPtr()[i]; }
    T& operator[](SIZE_TYPE i) { return getPtr()[i]; }

    void reserve(SIZE_TYPE capacity) { ensureCapacity(capacity); }

    void resize(SIZE_TYPE size, T value = T()) {
        if (mSize == size) {
            return;
        } else if (mSize > size) {  // reduce size
            if (isArrayUsed()) {    // array to array reduction, so no need to reallocate.
                mSize = size;
            } else if (size > ARRAY_CAPACITY) {  // heap to heap reduction
                T* newPtr = new T[size];
                const T* oldPtr = getPtr();
                std::copy(oldPtr, oldPtr + size, newPtr);
                free();
                mArray[0] = reinterpret_cast<uintptr_t>(newPtr);
                mSize = size;
                mCapacity = size;
            } else {  // heap to array reduction.
                const T* oldPtr = getPtr();
                T* newPtr = reinterpret_cast<T*>(&mArray[0]);
                std::copy(oldPtr, oldPtr + size, newPtr);
                delete[] oldPtr;  // we cannot use free() here because we wrote data to mArray.
                mSize = size;
                mCapacity = ARRAY_CAPACITY;
            }
        } else {  // mSize < size  // increase size
            ensureCapacity(size);
            T* ptr = getPtr();
            for (SIZE_TYPE i = mSize; i < size; ++i) {
                ptr[i] = value;
            }
            mSize = size;
        }
    }

    void push_back(const T& x) {
        if (mSize >= mCapacity) [[unlikely]] {
            // exponential backoff
            constexpr SIZE_TYPE kMaxIncrease = static_cast<SIZE_TYPE>(4096 / sizeof(T));
            ensureCapacity(mCapacity + std::min(mCapacity, kMaxIncrease));
        }
        *(getPtr() + mSize) = x;
        mSize++;
    }

    void shrink_to_fit() {
        if (mSize == mCapacity || mCapacity == ARRAY_CAPACITY) {
            return;
        }

        bool needFree = !isArrayUsed();

        const T* oldPtr = getPtr();
        T* newPtr;
        if (mSize <= ARRAY_CAPACITY) {
            newPtr = reinterpret_cast<T*>(&mArray[0]);
            mCapacity = ARRAY_CAPACITY;
            std::copy(oldPtr, oldPtr + mSize, newPtr);
        } else {
            newPtr = new T[mSize];
            mCapacity = mSize;
            std::copy(oldPtr, oldPtr + mSize, newPtr);
            mArray[0] = reinterpret_cast<uintptr_t>(newPtr);
        }
        if (needFree) {
            delete[] oldPtr;
        }
    }

    void clear() {
        mSize = 0;  // don't free up until free is called.
    }

    bool empty() const { return mSize == 0; }

    SIZE_TYPE size() const { return mSize; }
    SIZE_TYPE capacity() const { return mCapacity; }

private:
    uintptr_t mArray[PTR_ARRAY_SIZE];
    SIZE_TYPE mSize;
    SIZE_TYPE mCapacity;

    void copy(const T* src, SIZE_TYPE count) {
        clear();
        ensureCapacity(count);
        mSize = count;
        memcpy(getPtr(), src, count * sizeof(T));
    }

    void move(PackedVector&& o) {
        mSize = o.mSize;
        o.mSize = 0;
        mCapacity = o.mCapacity;
        o.mCapacity = ARRAY_CAPACITY;
        for (uint32_t i = 0; i < PTR_ARRAY_SIZE; ++i) {
            mArray[i] = o.mArray[i];
            o.mArray[i] = 0;
        }
    }

    inline bool isArrayUsed() const { return mCapacity <= ARRAY_CAPACITY; }

    void ensureCapacity(SIZE_TYPE capacity) {
        if (capacity <= mCapacity) {
            return;
        }

        if (capacity > ARRAY_CAPACITY) {
            T* newPtr = new T[capacity];
            const T* oldPtr = getPtr();
            std::copy(oldPtr, oldPtr + mSize, newPtr);
            free();
            mArray[0] = reinterpret_cast<uintptr_t>(newPtr);
            mCapacity = capacity;
        } else {
            mCapacity = ARRAY_CAPACITY;
        }
    }

    void free() {
        if (!isArrayUsed()) {
            delete[] reinterpret_cast<T*>(mArray[0]);
            mArray[0] = 0;
            mCapacity = ARRAY_CAPACITY;
        }
    }

    inline T* getPtr() {
        return isArrayUsed() ? reinterpret_cast<T*>(&mArray[0]) : reinterpret_cast<T*>(mArray[0]);
    }

    inline const T* getPtr() const {
        return isArrayUsed() ? reinterpret_cast<const T*>(&mArray[0])
                             : reinterpret_cast<const T*>(mArray[0]);
    }

public:
    inline const T* begin() const { return getPtr(); }
    inline const T* end() const { return getPtr() + mSize; }
    inline const T* back() const { return getPtr() + mSize - 1; }
    inline T* begin() { return getPtr(); }
    inline T* end() { return getPtr() + mSize; }
    inline T* back() { return getPtr() + mSize - 1; }
};

}  // namespace minikin
#endif  // MINIKIN_PACKED_VECTOR_H
