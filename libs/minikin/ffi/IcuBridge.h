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

#ifndef MINIKIN_ICU_BRIDGE_H
#define MINIKIN_ICU_BRIDGE_H

#include <cstdint>

namespace minikin {
namespace rust {

/**
 * Delegate ICU4C uscript_getScript API for Rust.
 * TODO: Replace with ICU4X once it becomes available in Android.
 */
uint8_t getScript(uint32_t codePoint);

/**
 * Delegate ICU4C u_getIntPropertyValue with UCHAR_JOINING_TYPE API for Rust.
 * TODO: Replace with ICU4X once it becomes available in Android.
 */
uint8_t getJoiningType(uint32_t codePoint);
}  // namespace rust

}  // namespace minikin

#endif  // MINIKIN_LETTER_SPACING_UTILS_H
