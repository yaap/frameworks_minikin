/*
 * Copyright (C) 2025 The Android Open Source Project
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

#ifndef MINIKIN_LAYOUT_CONTEXT_H
#define MINIKIN_LAYOUT_CONTEXT_H

#include <hb.h>
#include <minikin/MinikinExtent.h>

#include <map>

namespace minikin {

class MinikinFont;

using ScriptExtentCache = std::map<hb_script_t, MinikinExtent>;

// A class that holds the context of the text layout calculation.
// The meaning context here is the same Paint parameters. So, the same context should not be used
// for the different text size, typeface, etc.
struct LayoutContext {
    // A cache of the script specific extent for the given font.
    std::map<MinikinFont*, ScriptExtentCache> scriptExtentCache;

    // A cache of the extent for the given font.
    std::map<MinikinFont*, MinikinExtent> extentCache;
};

}  // namespace minikin

#endif  // MINIKIN_LAYOUT_CONTEXT_H
