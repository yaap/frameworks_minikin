/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef MINIKIN_FEATURE_FLAGS_H
#define MINIKIN_FEATURE_FLAGS_H

#ifdef USE_FEATURE_FLAGS
#include <com_android_text_flags.h>
#endif  // USE_FEATURE_FLAGS

namespace features {

#ifdef USE_FEATURE_FLAGS

inline bool phrase_strict_fallback() {
    return com_android_text_flags_phrase_strict_fallback();
}

#else

inline bool phrase_strict_fallback() {
    return true;
}

#endif  // USE_FEATURE_FLAGS

}  // namespace features

#endif  // FEATURE_FLAGS
