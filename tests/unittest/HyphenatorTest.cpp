/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "FeatureFlags.h"
#include "FileUtils.h"
#include "minikin/Hyphenator.h"

#ifndef NELEM
#define NELEM(x) ((sizeof(x) / sizeof((x)[0])))
#endif

namespace minikin {

const char* usHyph = "/system/usr/hyphen-data/hyph-en-us.hyb";
const char* ptHyph = "/system/usr/hyphen-data/hyph-pt.hyb";
const char* malayalamHyph = "/system/usr/hyphen-data/hyph-ml.hyb";

const uint16_t HYPHEN_MINUS = 0x002D;
const uint16_t SOFT_HYPHEN = 0x00AD;
const uint16_t MIDDLE_DOT = 0x00B7;
const uint16_t GREEK_LOWER_ALPHA = 0x03B1;
const uint16_t ARMENIAN_AYB = 0x0531;
const uint16_t HEBREW_ALEF = 0x05D0;
const uint16_t ARABIC_ALEF = 0x0627;
const uint16_t ARABIC_BEH = 0x0628;
const uint16_t ARABIC_ZWARAKAY = 0x0659;
const uint16_t MALAYALAM_KA = 0x0D15;
const uint16_t UCAS_E = 0x1401;
const uint16_t HYPHEN = 0x2010;
const uint16_t EN_DASH = 0x2013;

typedef std::function<Hyphenator*(const uint8_t*, size_t, size_t, size_t, const std::string&)>
        Generator;

class HyphenatorTest : public testing::TestWithParam<Generator> {};

INSTANTIATE_TEST_SUITE_P(HyphenatorInstantiation, HyphenatorTest,
                         testing::Values(HyphenatorCXX::loadBinary, Hyphenator::loadBinaryForRust),
                         [](const testing::TestParamInfo<HyphenatorTest::ParamType>& info) {
                             switch (info.index) {
                                 case 0:
                                     return "CXX";
                                 case 1:
                                     return "Rust";
                                 default:
                                     return "Unknown";
                             }
                         });

// Simple test for US English. This tests "table", which happens to be the in the exceptions list.
TEST_P(HyphenatorTest, usEnglishAutomaticHyphenation) {
    std::vector<uint8_t> patternData = readWholeFile(usHyph);
    Hyphenator* hyphenator = GetParam()(patternData.data(), patternData.size(), 2, 3, "en");
    const uint16_t word[] = {'t', 'a', 'b', 'l', 'e'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)5, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[3]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[4]);
}

// Catalan l·l should break as l-/l
TEST_P(HyphenatorTest, catalanMiddleDot) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "ca");
    const uint16_t word[] = {'l', 'l', MIDDLE_DOT, 'l', 'l'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)5, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[2]);
    EXPECT_EQ(HyphenationType::BREAK_AND_REPLACE_WITH_HYPHEN, result[3]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[4]);
}

// Catalan l·l should not break if the word is too short.
TEST_P(HyphenatorTest, catalanMiddleDotShortWord) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "ca");
    const uint16_t word[] = {'l', MIDDLE_DOT, 'l'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[2]);
}

// If we break on a hyphen in Polish, the hyphen should be repeated on the next line.
TEST_P(HyphenatorTest, polishHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "pl");
    const uint16_t word[] = {'x', HYPHEN, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN_AT_NEXT_LINE, result[2]);
}

// If the language is Polish but the script is not Latin, don't use Polish rules for hyphenation.
TEST_P(HyphenatorTest, polishHyphenButNonLatinWord) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "pl");
    const uint16_t word[] = {GREEK_LOWER_ALPHA, HYPHEN, GREEK_LOWER_ALPHA};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
}

// Polish en dash doesn't repeat on next line (as far as we know), but just provides a break
// opportunity.
TEST_P(HyphenatorTest, polishEnDash) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "pl");
    const uint16_t word[] = {'x', EN_DASH, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
}

// If we break on a hyphen in Slovenian, the hyphen should be repeated on the next line. (Same as
// Polish.)
TEST_P(HyphenatorTest, slovenianHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "sl");
    const uint16_t word[] = {'x', HYPHEN, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN_AT_NEXT_LINE, result[2]);
}

// In Latin script text, soft hyphens should insert a visible hyphen if broken at.
TEST_P(HyphenatorTest, latinSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {'x', SOFT_HYPHEN, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
}

// Soft hyphens at the beginning of a word are not useful in linebreaking.
TEST_P(HyphenatorTest, latinSoftHyphenStartingTheWord) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {SOFT_HYPHEN, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)2, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
}

// In Malayalam script text, soft hyphens should not insert a visible hyphen if broken at.
TEST_P(HyphenatorTest, malayalamSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {MALAYALAM_KA, SOFT_HYPHEN, MALAYALAM_KA};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
}

// In automatically hyphenated Malayalam script text, we should not insert a visible hyphen.
TEST_P(HyphenatorTest, malayalamAutomaticHyphenation) {
    std::vector<uint8_t> patternData = readWholeFile(malayalamHyph);
    Hyphenator* hyphenator = GetParam()(patternData.data(), patternData.size(), 2, 2, "en");
    const uint16_t word[] = {MALAYALAM_KA, MALAYALAM_KA, MALAYALAM_KA, MALAYALAM_KA, MALAYALAM_KA};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)5, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[3]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[4]);
}

// In Armenian script text, soft hyphens should insert an Armenian hyphen if broken at.
TEST_P(HyphenatorTest, aremenianSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARMENIAN_AYB, SOFT_HYPHEN, ARMENIAN_AYB};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_ARMENIAN_HYPHEN, result[2]);
}

// In Hebrew script text, soft hyphens should insert a normal hyphen if broken at, for now.
// We may need to change this to maqaf later.
TEST_P(HyphenatorTest, hebrewSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {HEBREW_ALEF, SOFT_HYPHEN, HEBREW_ALEF};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
}

// Soft hyphen between two Arabic letters that join should keep the joining
// behavior when broken across lines.
TEST_P(HyphenatorTest, arabicSoftHyphenConnecting) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARABIC_BEH, SOFT_HYPHEN, ARABIC_BEH};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN_AND_ZWJ, result[2]);
}

// Arabic letters may be joining on one side, but if it's the wrong side, we
// should use the normal hyphen.
TEST_P(HyphenatorTest, arabicSoftHyphenNonConnecting) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARABIC_ALEF, SOFT_HYPHEN, ARABIC_BEH};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
}

// Skip transparent characters until you find a non-transparent one.
TEST_P(HyphenatorTest, arabicSoftHyphenSkipTransparents) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARABIC_BEH, ARABIC_ZWARAKAY, SOFT_HYPHEN, ARABIC_ZWARAKAY, ARABIC_BEH};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)5, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[2]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN_AND_ZWJ, result[3]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[4]);
}

// Skip transparent characters until you find a non-transparent one. If we get to one end without
// finding anything, we are still non-joining.
TEST_P(HyphenatorTest, arabicSoftHyphenTransparentsAtEnd) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARABIC_BEH, ARABIC_ZWARAKAY, SOFT_HYPHEN, ARABIC_ZWARAKAY};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)4, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[2]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[3]);
}

// Skip transparent characters until you find a non-transparent one. If we get to one end without
// finding anything, we are still non-joining.
TEST_P(HyphenatorTest, arabicSoftHyphenTransparentsAtStart) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {ARABIC_ZWARAKAY, SOFT_HYPHEN, ARABIC_ZWARAKAY, ARABIC_BEH};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)4, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[3]);
}

// In Unified Canadian Aboriginal script (UCAS) text, soft hyphens should insert a UCAS hyphen.
TEST_P(HyphenatorTest, ucasSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {UCAS_E, SOFT_HYPHEN, UCAS_E};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_UCAS_HYPHEN, result[2]);
}

// Presently, soft hyphen looks at the character after it to determine hyphenation type. This is a
// little arbitrary, but let's test it anyway.
TEST_P(HyphenatorTest, mixedScriptSoftHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {'a', SOFT_HYPHEN, UCAS_E};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_UCAS_HYPHEN, result[2]);
}

// Hard hyphens provide a breaking opportunity with nothing extra inserted.
TEST_P(HyphenatorTest, hardHyphen) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {'x', HYPHEN, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
}

// Hyphen-minuses also provide a breaking opportunity with nothing extra inserted.
TEST_P(HyphenatorTest, hyphenMinus) {
    Hyphenator* hyphenator = GetParam()(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {'x', HYPHEN_MINUS, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)3, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[2]);
}

// If the word starts with a hard hyphen or hyphen-minus, it doesn't make sense to break
// it at that point.
TEST_P(HyphenatorTest, startingHyphenMinus) {
    Hyphenator* hyphenator = Hyphenator::loadBinary(nullptr, 0, 2, 2, "en");
    const uint16_t word[] = {HYPHEN_MINUS, 'y'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)2, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
}

TEST_P(HyphenatorTest, hyphenationWithHyphen) {
    std::vector<uint8_t> patternData = readWholeFile(ptHyph);
    Hyphenator* hyphenator = GetParam()(patternData.data(), patternData.size(), 2, 3, "pt");
    const uint16_t word[] = {'b', 'o', 'a', 's', '-', 'v', 'i', 'n', 'd', 'a', 's'};
    std::vector<HyphenationType> result;
    hyphenator->hyphenate(word, &result);
    EXPECT_EQ((size_t)11, result.size());
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[0]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[1]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[2]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[3]);
    EXPECT_EQ(HyphenationType::BREAK_AND_DONT_INSERT_HYPHEN, result[4]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[5]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[6]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[7]);
    EXPECT_EQ(HyphenationType::BREAK_AND_INSERT_HYPHEN, result[8]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[9]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[10]);
    EXPECT_EQ(HyphenationType::DONT_BREAK, result[11]);
}

}  // namespace minikin
