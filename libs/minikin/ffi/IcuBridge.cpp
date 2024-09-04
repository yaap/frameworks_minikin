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

#include "ffi/IcuBridge.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace minikin {

namespace rust {
// The following U_JT_ constants must be same to the ones defined in
// frameworks/minikin/rust/hyphenator.rs
// TODO: Remove this file once ICU4X API becomes available in Rust.
const uint8_t RUST_U_JT_NON_JOINING = 0;
const uint8_t RUST_U_JT_DUAL_JOINING = 1;
const uint8_t RUST_U_JT_RIGHT_JOINING = 2;
const uint8_t RUST_U_JT_LEFT_JOINING = 3;
const uint8_t RUST_U_JT_JOIN_CAUSING = 4;
const uint8_t RUST_U_JT_TRANSPARENT = 5;

// The following USCRIPT_ constants must be same to the ones defined in
// frameworks/minikin/rust/hyphenator.rs
// TODO: Remove this file once ICU4X API becomes available in Rust.
const uint8_t RUST_USCRIPT_LATIN = 0;
const uint8_t RUST_USCRIPT_ARABIC = 1;
const uint8_t RUST_USCRIPT_KANNADA = 2;
const uint8_t RUST_USCRIPT_MALAYALAM = 3;
const uint8_t RUST_USCRIPT_TAMIL = 4;
const uint8_t RUST_USCRIPT_TELUGU = 5;
const uint8_t RUST_USCRIPT_ARMENIAN = 6;
const uint8_t RUST_USCRIPT_CANADIAN_ABORIGINAL = 7;
const uint8_t RUST_USCRIPT_INVALID_CODE = 8;

uint8_t getScript(uint32_t codePoint) {
    UErrorCode errorCode = U_ZERO_ERROR;
    const UScriptCode script = uscript_getScript(static_cast<UChar32>(codePoint), &errorCode);
    if (U_FAILURE(errorCode)) {
        return RUST_USCRIPT_INVALID_CODE;
    }
    switch (script) {
        case USCRIPT_LATIN:
            return RUST_USCRIPT_LATIN;
        case USCRIPT_ARABIC:
            return RUST_USCRIPT_ARABIC;
        case USCRIPT_KANNADA:
            return RUST_USCRIPT_KANNADA;
        case USCRIPT_MALAYALAM:
            return RUST_USCRIPT_MALAYALAM;
        case USCRIPT_TAMIL:
            return RUST_USCRIPT_TAMIL;
        case USCRIPT_TELUGU:
            return RUST_USCRIPT_TELUGU;
        case USCRIPT_ARMENIAN:
            return RUST_USCRIPT_ARMENIAN;
        case USCRIPT_CANADIAN_ABORIGINAL:
            return RUST_USCRIPT_CANADIAN_ABORIGINAL;
        default:
            return RUST_USCRIPT_INVALID_CODE;
    }
}

uint8_t getJoiningType(uint32_t codePoint) {
    int32_t joiningType = u_getIntPropertyValue(codePoint, UCHAR_JOINING_TYPE);
    switch (joiningType) {
        case U_JT_NON_JOINING:
            return RUST_U_JT_NON_JOINING;
        case U_JT_DUAL_JOINING:
            return RUST_U_JT_DUAL_JOINING;
        case U_JT_RIGHT_JOINING:
            return RUST_U_JT_RIGHT_JOINING;
        case U_JT_LEFT_JOINING:
            return RUST_U_JT_LEFT_JOINING;
        case U_JT_JOIN_CAUSING:
            return RUST_U_JT_JOIN_CAUSING;
        case U_JT_TRANSPARENT:
            return RUST_U_JT_TRANSPARENT;
        default:
            return RUST_U_JT_NON_JOINING;
    }
}

}  // namespace rust
}  // namespace minikin
