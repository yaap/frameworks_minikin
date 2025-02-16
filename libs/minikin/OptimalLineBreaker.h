/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef MINIKIN_OPTIMAL_LINE_BREAKER_H
#define MINIKIN_OPTIMAL_LINE_BREAKER_H

#include "minikin/LineBreaker.h"
#include "minikin/MeasuredText.h"
#include "minikin/U16StringPiece.h"

namespace minikin {

LineBreakResult breakLineOptimal(const U16StringPiece& textBuf, const MeasuredText& measured,
                                 const LineWidth& lineWidthLimits, BreakStrategy strategy,
                                 HyphenationFrequency frequency, bool justified,
                                 bool useBoundsForWidth);

}  // namespace minikin

#endif  // MINIKIN_OPTIMAL_LINE_BREAKER_H
