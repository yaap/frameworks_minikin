/*
 * Copyright 2024 The Android Open Source Project
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

use std::cmp;

/// An implementation of hyphenation for Android.
///
/// The hyphenation in Android is done with two steps: first performs the Knuth-Liang hyphenation
/// algorithm for populating all possible hyphenation points. Then, resolve hyphenation type from
/// the scripts and locales.
///
/// The Knuth-Liang hyphenation works like as follows:
/// The Knuth-Liang hyphenation uses two dictionary: pattern dictionary and exception files. The
/// files end with ".hyp.txt" are exception files and the files end with ".pat.txt" are pattern
/// files. If the word is in exception file, the hyphenation is performed as it is specified in the
/// exception file.
///
/// Then, if the word is not in the exception file, the Knuth-Liang hyphenation is performed with
/// hyphenation pattern dictionary. The hyphenation pattern dictionary is a list of sub-word with
/// hyphenation level as number. The level values are assigned between each letters including before
/// the first letter and last letter. If the value is odd number, then the position is a hyphenation
/// point. If the value is even number, then the position is not a hyphenation point. In the pattern
/// file, the 0 is dropped, so the meaning of "re4ti4z" is level values "0040040" for sub-word
/// "retiz". The hyphenation is performed by iterating all patterns and assigning level values to
/// the possible break points. If the break point can be assigned from multiple patterns, the
/// maximum value is used. If none of the pattern matches the break point, the level is zero,
/// therefore do not break. And finally the odd numbered positions are the break points.
///
/// Here is an example how the "hyphenation" is broken into "hy-phen-ation".
/// The relevant patterns in the pattern dictionary are
/// - hy3ph
/// - he2n
/// - hena4
/// - hen5at
/// - 1na
/// - n2at
/// - 1tio
/// - 2io
/// - o2n
///
/// Then when these patterns are applied to the word "hyphenation", it becomes like
///
///   h y p h e n a t i o n
///  0 0 3 0 0              : hy3ph
///        0 0 2 0          : he2n
///        0 0 0 0 4        : hena4
///        0 0 0 5 0 0      : hen5at
///            1 0 0        : 1na
///            0 2 0 0      : n2at
///                1 0 0 0  : 1tio
///                  2 0 0  : 2io
///                    0 2 0: o2n
/// ---------------------------------
///  0 0 3 0 0 2 5 4 2 0 2 0: max
///
/// Then, the odd-numbered break points are hyphenation allowed break points, so the result is
/// "hy-phen-ation".
///
/// In the Android implementation, the hyphenation pattern file is preprocessed to Trie in build
/// time. For the detailed file format, see comments of HyphenationData struct.
///
/// Once the all possible hyphenation break points are collected, the decide the hyphenation break
/// type is determined based on the script and locale. For example, in case of Arabic, the letter
/// form should not be changed by hyphenation, so ZWJ can be inserted before and after hyphen
/// letter.

const CHAR_SOFT_HYPHEN: u16 = 0x00AD;
const CHAR_MIDDLE_DOT: u16 = 0x00B7;
const CHAR_HYPHEN_MINUS: u16 = 0x002D;
const CHAR_HYPHEN: u16 = 0x2010;

// The following U_JT_* constants must be same to the ones defined in
// frameworks/minikin/lib/minikin/ffi/IciBridge.h
// TODO: Replace with ICU4X once it becomes available in Android.
const U_JT_NON_JOINING: u8 = 0;
const U_JT_DUAL_JOINING: u8 = 1;
const U_JT_RIGHT_JOINING: u8 = 2;
const U_JT_LEFT_JOINING: u8 = 3;
const U_JT_JOIN_CAUSING: u8 = 4;
const U_JT_TRANSPARENT: u8 = 5;

// The following USCRIPT_* constants must be same to the ones defined in
// frameworks/minikin/lib/minikin/ffi/IciBridge.h
// TODO: Replace with ICU4X once it becomes available in Android.
const USCRIPT_LATIN: u8 = 0;
const USCRIPT_ARABIC: u8 = 1;
const USCRIPT_KANNADA: u8 = 2;
const USCRIPT_MALAYALAM: u8 = 3;
const USCRIPT_TAMIL: u8 = 4;
const USCRIPT_TELUGU: u8 = 5;
const USCRIPT_ARMENIAN: u8 = 6;
const USCRIPT_CANADIAN_ABORIGINAL: u8 = 7;

use crate::ffi::getJoiningType;
use crate::ffi::getScript;

#[cfg(target_os = "android")]
fn portuguese_hyphenator() -> bool {
    android_text_flags::portuguese_hyphenator()
}

#[cfg(not(target_os = "android"))]
fn portuguese_hyphenator() -> bool {
    true
}

/// Hyphenation types
/// The following values must be equal to the ones in
/// frameworks/minikin/include/minikin/Hyphenator.h
#[repr(u8)]
#[derive(PartialEq, Copy, Clone)]
pub enum HyphenationType {
    /// Do not break.
    DontBreak = 0,
    /// Break the line and insert a normal hyphen.
    BreakAndInsertHyphen = 1,
    /// Break the line and insert an Armenian hyphen (U+058A).
    BreakAndInsertArmenianHyphen = 2,
    /// Break the line and insert a Canadian Syllabics hyphen (U+1400).
    BreakAndInsertUcasHyphen = 4,
    /// Break the line, but don't insert a hyphen. Used for cases when there is already a hyphen
    /// present or the script does not use a hyphen (e.g. in Malayalam).
    BreakAndDontInsertHyphen = 5,
    /// Break and replace the last code unit with hyphen. Used for Catalan "l·l" which hyphenates
    /// as "l-/l".
    BreakAndReplaceWithHyphen = 6,
    /// Break the line, and repeat the hyphen (which is the last character) at the beginning of the
    /// next line. Used in Polish (where "czerwono-niebieska" should hyphenate as
    /// "czerwono-/-niebieska") and Slovenian.
    BreakAndInsertHyphenAtNextLine = 7,
    /// Break the line, insert a ZWJ and hyphen at the first line, and a ZWJ at the second line.
    /// This is used in Arabic script, mostly for writing systems of Central Asia. It's our default
    /// behavior when a soft hyphen is used in Arabic script.
    BreakAndInsertHyphenAndZwj = 8,
}

/// Hyphenation locale
#[repr(u8)]
#[derive(PartialEq, Copy, Clone)]
pub enum HyphenationLocale {
    /// Other locale
    Other = 0,
    /// Catalan
    Catalan = 1,
    /// Polish
    Polish = 2,
    /// Slovenian
    Slovenian = 3,
    /// Portuguese
    Portuguese = 4,
}

const MAX_HYPHEN_SIZE: u32 = 64;

struct HyphenationData<'a> {
    bytes: &'a [u8],
}

/// The Hyphenation pattern file is encoded into binary format during build time.
/// The hyphenation pattern file is encoded into three objects: AlphabetTable, Trie, Patterns.
///
/// First, to avoid high value of utf16 char values in Trie object, char values are mapped to
/// internal alphabet codes. The AlphabetTable0 and AndroidTable1 has a map from utf16 char values
/// to internal u16 alphabet codes. The AlphabetTable0 is used if the min and max used code points
/// has less than 1024, i.e. max_codepoint - min_codepoint < 1024. The AlphabetTable1 is used
/// otherwise.
///
/// Then, the pattern file is encoded with Trie and Pattern object with using internal
/// alphabet code. For example, in case of the entry "ef5i5nite", the hyphenation score "00550000"
/// is stored in the Pattern object and the subword "efinite" is stored in the Trie object.
///
/// The Trie object is encoded as one dimensional u32 arrays. Each u32 integer contains packed
/// index to the Pattern object, index to the next node entry and alphabet code.
/// Trie Entry:
///    0                   1                   2                   3
///    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  (bits)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |   index to pattern data   |  index to the next node |   code  |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   Note: the layout is as an example of pattern_shift = 19, link_shift = 5.
///
/// The Pattern object is encoded into two data: entry list and data payload. The entry is a packed
/// u32 integer that contains length of the pattern, an amount of shift of the pattern index and
/// an offset from the payload head.
///
/// Pattern Entry:
///    0                   1                   2                   3
///    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  (bits)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |len of pat | pat shift |      offset to the pattern data       |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///
/// The pattern data and related information can be obtained as follows:
///
///   // Pattern information
///   let entry = pattern[16 + pattern_index * 4]  // read u32 as little endian.
///   let pattern_length = entry >> 26
///   let pattern_shift = (entry > 20) & 0x3f
///
///   // Pattern value retrieval: i-th offset in the word.
///   let pattern_offset = pattern[8] // read u32 as little endian.
///   let pattern_value = pattern[pattern_offset + (entry & 0xfffff) + i]
impl<'a> HyphenationData<'a> {
    pub const fn new(bytes: &'a [u8]) -> Self {
        HyphenationData { bytes }
    }

    pub fn read_u32(&self, offset: u32) -> u32 {
        let usize_offset = offset as usize;
        self.bytes
            .get(usize_offset..usize_offset + 4)
            .map(|x: &[u8]| u32::from_le_bytes(x.try_into().unwrap()))
            .unwrap()
    }
}

/// Header struct of the hyphenation pattern file.
/// The object layout follows:
///    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    (bytes)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |      magic    |     version   |alphabet offset|  trie offset  |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |pattern offset |   file size   |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct Header<'a> {
    data: HyphenationData<'a>,
}

/// Alphabet Table version 0 struct of the hyphenation pattern file.
/// The object layout follows:
///    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    (bytes)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |     version   | min codepoint | max codepoint |    payload
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct AlphabetTable0<'a> {
    data: HyphenationData<'a>,
    min_codepoint: u32,
    max_codepoint: u32,
}

/// Alphabet Table version 1 struct of the hyphenation pattern file.
/// The object layout follows:
///    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    (bytes)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |     version   | num of entries|         payload
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct AlphabetTable1<'a> {
    data: HyphenationData<'a>,
    num_entries: u32,
}

/// An entry of alphabet table version 1 struct of the hyphenation pattern file.
/// The entry is packed u32 value: the high 21 bits are code point and low 11 bits
/// are alphabet code value.
///    0                   1                   2                   3
///    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  (bits)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |                code point               |     code value      |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct AlphabetTable1Entry {
    entry: u32,
}

/// Trie struct of the hyphenation pattern file.
/// The object layout follows:
///    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    (bytes)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |     version   |   char mask   |  link shift   |   link mask   |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   | pattern shift |  num entries  |         payload
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct Trie<'a> {
    data: HyphenationData<'a>,
}

/// Pattern struct of the hyphenation pattern file.
/// The object layout follows:
///    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F    (bytes)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |     version   | num entries   | pattern offset|  pattern size |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   | payload
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct Pattern<'a> {
    data: HyphenationData<'a>,
    pattern_offset: u32,
}

/// An entry of pattern struct of the hyphenation pattern file.
/// The entry is packed u32 value: the highest 6 bits are for length, next 6 bits are amount of
/// shift, and lowest 20 bits are offset of the first value from the pattern offset value.
///    0                   1                   2                   3
///    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  (bits)
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///   |   length  |   shift   |     offset of the first value         |
///   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
pub struct PatternEntry<'a> {
    data: HyphenationData<'a>,
    pattern_offset: u32,
    entry: u32,
}

impl<'a> Header<'a> {
    /// Construct a reader of the Header struct from the byte array.
    pub const fn new(bytes: &'a [u8]) -> Self {
        Header { data: HyphenationData::new(bytes) }
    }

    /// Returns the reader of the alphabet code.
    pub fn alphabet_table(&self) -> Option<Box<dyn AlphabetLookup + 'a>> {
        let offset = self.data.read_u32(8);
        let version = self.data.read_u32(offset);
        return match version {
            0 => Some(Box::new(AlphabetTable0::new(self.read_offset_and_slice(8)))),
            1 => Some(Box::new(AlphabetTable1::new(self.read_offset_and_slice(8)))),
            _ => None,
        };
    }

    /// Returns the reader of the trie struct.
    pub fn trie_table(&self) -> Trie<'a> {
        Trie::new(self.read_offset_and_slice(12))
    }

    /// Returns the reader of the pattern struct.
    pub fn pattern_table(&self) -> Pattern<'a> {
        Pattern::new(self.read_offset_and_slice(16))
    }

    fn read_offset_and_slice(&self, offset: u32) -> &'a [u8] {
        let offset = self.data.read_u32(offset) as usize;
        self.data.bytes.get(offset..).unwrap()
    }
}

pub trait AlphabetLookup {
    /// Get the alphabet code for the code point.
    fn get_at(&self, c: u32) -> Option<u16>;

    /// Lookup the internal alphabet codes from UTF-16 character codes.
    fn lookup(
        &self,
        alpha_codes: &mut [u16; MAX_HYPHEN_SIZE as usize],
        word: &[u16],
    ) -> HyphenationType {
        let mut result = HyphenationType::BreakAndInsertHyphen;
        alpha_codes[0] = 0; // word start
        for i in 0..word.len() {
            let c = word[i] as u32;
            if let Some(code) = self.get_at(c) {
                alpha_codes[i + 1] = code;
            } else {
                return HyphenationType::DontBreak;
            }
            if result == HyphenationType::BreakAndInsertHyphen {
                result = Hyphenator::hyphenation_type_based_on_script(c);
            }
        }
        alpha_codes[word.len() + 1] = 0; // word termination
        result
    }
}

/// Map from utf16 code unit to the internal alphabet code.
impl<'a> AlphabetTable0<'a> {
    /// Construct a reader of the Alphabet Table version 0 struct from the byte array.
    pub fn new(bytes: &'a [u8]) -> Self {
        let data = HyphenationData::new(bytes);
        let min_codepoint = data.read_u32(4);
        let max_codepoint = data.read_u32(8);
        AlphabetTable0 { data, min_codepoint, max_codepoint }
    }
}

impl<'a> AlphabetLookup for AlphabetTable0<'a> {
    /// Returns an entry of the specified offset.
    fn get_at(&self, offset: u32) -> Option<u16> {
        if offset < self.min_codepoint || offset >= self.max_codepoint {
            None
        } else {
            let code = self.data.bytes[(offset - self.min_codepoint) as usize + 12] as u16;
            if code == 0 {
                None
            } else {
                Some(code)
            }
        }
    }
}

/// Map from utf16 code unit to the internal alphabet code.
impl<'a> AlphabetTable1<'a> {
    /// Construct a reader of the Alphabet Table version 1 struct from the byte array.
    pub fn new(bytes: &'a [u8]) -> Self {
        let data = HyphenationData::new(bytes);
        let num_entries = data.read_u32(4);
        AlphabetTable1 { data, num_entries }
    }

    fn lower_bounds(&self, value: u32) -> Option<u32> {
        let mut b = 0;
        let mut e = self.num_entries;
        while b != e {
            let m = b + (e - b) / 2;
            let c = self.data.read_u32(8 + m * 4);
            if c >= value {
                e = m;
            } else {
                b = m + 1;
            }
        }
        if b == self.num_entries {
            None
        } else {
            Some(b)
        }
    }
}

impl<'a> AlphabetLookup for AlphabetTable1<'a> {
    fn get_at(&self, c: u32) -> Option<u16> {
        if let Some(r) = self.lower_bounds(c << 11) {
            let entry = AlphabetTable1Entry::new(self.data.read_u32(8 + r * 4));
            if entry.codepoint() == c {
                Some(entry.value())
            } else {
                None
            }
        } else {
            None
        }
    }
}

/// A packed u32 entry of the AlphabetTable1.
impl AlphabetTable1Entry {
    pub const fn new(entry_value: u32) -> Self {
        AlphabetTable1Entry { entry: entry_value }
    }

    /// Unpack code point from entry value.
    pub fn codepoint(&self) -> u32 {
        self.entry >> 11
    }

    /// Unpack value from entry value.
    pub fn value(&self) -> u16 {
        (self.entry & 0x7ff).try_into().unwrap()
    }
}

/// A Trie object.
/// See the function comment of HyphenationData for the details.
impl<'a> Trie<'a> {
    /// Construct a reader of the Trie struct from the byte array.
    pub const fn new(bytes: &'a [u8]) -> Self {
        Trie { data: HyphenationData::new(bytes) }
    }

    /// Returns an entry of at the offset.
    /// The entry of the next alphabet code is
    ///
    /// let entry = trie.get_at(node + alphabet_codes[char])
    pub fn get_at(&self, offset: u32) -> u32 {
        self.data.read_u32(24 + offset * 4)
    }

    /// Returns the bit mask for the character code point of the node.
    /// You can get node's character code point by
    ///
    /// let node_character = entry & char_mask.
    pub fn char_mask(&self) -> u32 {
        self.data.read_u32(4)
    }

    /// Returns the amount of shift of the node index.
    /// You can get node number as following
    ///
    /// let next_node = (entry & link_mask) >> link_shift
    pub fn link_shift(&self) -> u32 {
        self.data.read_u32(8)
    }

    /// Returns the mask for the node index.
    /// You can get node number as following
    ///
    /// let next_node = (entry & link_mask) >> link_shift
    pub fn link_mask(&self) -> u32 {
        self.data.read_u32(12)
    }

    /// Returns the amount of shift of the pattern index.
    /// You can get pattern index as following
    ///
    /// let pattern_index = entry >> pattern_shift
    pub fn pattern_shift(&self) -> u32 {
        self.data.read_u32(16)
    }
}

/// A Pattern object.
/// See the function comment of HyphenationData for the details.
impl<'a> Pattern<'a> {
    /// Construct a reader of the Pattern struct from the byte array.
    pub fn new(bytes: &'a [u8]) -> Self {
        let data = HyphenationData::new(bytes);
        let pattern_offset = data.read_u32(8);
        Pattern { data, pattern_offset }
    }

    /// Returns a packed u32 entry at the given offset.
    pub fn entry_at(&self, offset: u32) -> PatternEntry<'a> {
        let entry = self.data.read_u32(16 + offset * 4);
        PatternEntry::new(self.data.bytes, self.pattern_offset, entry)
    }
}

/// An entry of the pattern object.
impl<'a> PatternEntry<'a> {
    /// Construct a reader of the Pattern struct from the byte array.
    pub const fn new(bytes: &'a [u8], pattern_offset: u32, entry: u32) -> Self {
        PatternEntry { data: HyphenationData::new(bytes), pattern_offset, entry }
    }

    /// Unpack length of the pattern from the packed entry value.
    pub fn len(&self) -> u32 {
        self.entry >> 26
    }

    /// Unpack an amount of shift of the pattern data from the packed entry value.
    pub fn shift(&self) -> u32 {
        (self.entry >> 20) & 0x3f
    }

    /// Returns a hyphenation score value at the offset in word with the entry.
    pub fn value_at(&self, offset: u32) -> u8 {
        self.data.bytes[(self.pattern_offset + (self.entry & 0xfffff) + offset) as usize]
    }
}

/// Performs hyphenation
pub struct Hyphenator {
    data: &'static [u8],
    min_prefix: u32,
    min_suffix: u32,
    locale: HyphenationLocale,
}

impl Hyphenator {
    /// Create a new hyphenator instance
    pub fn new(data: &'static [u8], min_prefix: u32, min_suffix: u32, locale: &str) -> Self {
        logger::init(
            logger::Config::default()
                .with_tag_on_device("Minikin")
                .with_max_level(log::LevelFilter::Trace),
        );
        Self {
            data,
            min_prefix,
            min_suffix,
            locale: if locale == "pl" {
                HyphenationLocale::Polish
            } else if locale == "ca" {
                HyphenationLocale::Catalan
            } else if locale == "sl" {
                HyphenationLocale::Slovenian
            } else if locale == "pt" {
                HyphenationLocale::Portuguese
            } else {
                HyphenationLocale::Other
            },
        }
    }

    /// Performs a hyphenation
    pub fn hyphenate(&self, word: &[u16], out: &mut [u8]) {
        let len: u32 = word.len().try_into().unwrap();
        let padded_len = len + 2;
        if !self.data.is_empty()
            && len >= self.min_prefix + self.min_suffix
            && padded_len <= MAX_HYPHEN_SIZE
        {
            let header = Header::new(self.data);
            let mut alpha_codes: [u16; MAX_HYPHEN_SIZE as usize] = [0; MAX_HYPHEN_SIZE as usize];
            let hyphen_value = if let Some(alphabet) = header.alphabet_table() {
                alphabet.lookup(&mut alpha_codes, word)
            } else {
                HyphenationType::DontBreak
            };

            if hyphen_value != HyphenationType::DontBreak {
                self.hyphenate_from_codes(alpha_codes, padded_len, hyphen_value, word, out);
                return;
            }
            // TODO: try NFC normalization
            // TODO: handle non-BMP Unicode (requires remapping of offsets)
        }
        // Note that we will always get here if the word contains a hyphen or a soft hyphen, because
        // the alphabet is not expected to contain a hyphen or a soft hyphen character, so
        // alphabetLookup would return DONT_BREAK.
        self.hyphenate_with_no_pattern(word, out);
    }

    /// This function determines whether a character is like U+2010 HYPHEN in line breaking and
    /// usage: a character immediately after which line breaks are allowed, but words containing
    /// it should not be automatically hyphenated using patterns. This is a curated set, created by
    /// manually inspecting all the characters that have the Unicode line breaking property of BA or
    /// HY and seeing which ones are hyphens.
    fn is_line_breaking_hyphen(c: u16) -> bool {
        c == 0x002D ||  // HYPHEN-MINUS
            c == 0x058A ||  // ARMENIAN HYPHEN
            c == 0x05BE ||  // HEBREW PUNCTUATION MAQAF
            c == 0x1400 ||  // CANADIAN SYLLABICS HYPHEN
            c == 0x2010 ||  // HYPHEN
            c == 0x2013 ||  // EN DASH
            c == 0x2027 ||  // HYPHENATION POINT
            c == 0x2E17 ||  // DOUBLE OBLIQUE HYPHEN
            c == 0x2E40 // DOUBLE HYPHEN
    }

    /// Resolves the hyphenation type for Arabic text.
    /// In case of Arabic text, the letter form should not be changed by hyphenation.
    /// So, if the hyphenation is in the middle of the joining context, insert ZWJ for keeping the
    /// form from the original text.
    fn get_hyph_type_for_arabic(word: &[u16], location: u32) -> HyphenationType {
        let mut i = location;
        let mut join_type: u8 = U_JT_NON_JOINING;
        while i < word.len().try_into().unwrap() {
            join_type = getJoiningType(word[i as usize].into());
            if join_type != U_JT_TRANSPARENT {
                break;
            }
            i += 1;
        }
        if join_type == U_JT_DUAL_JOINING
            || join_type == U_JT_RIGHT_JOINING
            || join_type == U_JT_JOIN_CAUSING
        {
            // The next character is of the type that may join the last character. See if the last
            // character is also of the right type.
            join_type = U_JT_NON_JOINING;
            if i >= 2 {
                i = location - 2; // skip the soft hyphen
                loop {
                    join_type = getJoiningType(word[i as usize].into());
                    if join_type != U_JT_TRANSPARENT {
                        break;
                    }
                    if i == 0 {
                        break;
                    }
                    i -= 1;
                }
            }
            if join_type == U_JT_DUAL_JOINING
                || join_type == U_JT_LEFT_JOINING
                || join_type == U_JT_JOIN_CAUSING
            {
                return HyphenationType::BreakAndInsertHyphenAndZwj;
            }
        }
        HyphenationType::BreakAndInsertHyphen
    }

    /// Performs the hyphenation without pattern files.
    fn hyphenate_with_no_pattern(&self, word: &[u16], out: &mut [u8]) {
        let word_len: u32 = word.len().try_into().unwrap();
        out[0] = HyphenationType::DontBreak as u8;
        for i in 1..word_len {
            let prev_char = word[i as usize - 1];
            if i > 1 && Self::is_line_breaking_hyphen(prev_char) {
                if (prev_char == CHAR_HYPHEN_MINUS || prev_char == CHAR_HYPHEN)
                    && (self.locale == HyphenationLocale::Polish
                        || self.locale == HyphenationLocale::Slovenian)
                    && getScript(word[i as usize].into()) == USCRIPT_LATIN
                {
                    // In Polish and Slovenian, hyphens get repeated at the next line. To be safe,
                    // we will do this only if the next character is Latin.
                    out[i as usize] = HyphenationType::BreakAndInsertHyphenAtNextLine as u8;
                } else {
                    out[i as usize] = HyphenationType::BreakAndDontInsertHyphen as u8;
                }
            } else if i > 1 && prev_char == CHAR_SOFT_HYPHEN {
                // Break after soft hyphens, but only if they don't start the word (a soft hyphen
                // starting the word doesn't give any useful break opportunities). The type of the
                // break is based on the script of the character we break on.
                if getScript(word[i as usize].into()) == USCRIPT_ARABIC {
                    // For Arabic, we need to look and see if the characters around the soft hyphen
                    // actually join. If they don't, we'll just insert a normal hyphen.
                    out[i as usize] = Self::get_hyph_type_for_arabic(word, i) as u8;
                } else {
                    out[i as usize] =
                        Self::hyphenation_type_based_on_script(word[i as usize] as u32) as u8;
                }
            } else if prev_char == CHAR_MIDDLE_DOT
                && self.min_prefix < i
                && i <= word_len - self.min_suffix
                && ((word[i as usize - 2] == 'l' as u16 && word[i as usize] == 'l' as u16)
                    || (word[i as usize - 2] == 'L' as u16 && word[i as usize] == 'L' as u16))
                && self.locale == HyphenationLocale::Catalan
            {
                // In Catalan, "l·l" should break as "l-" on the first line
                // and "l" on the next line.
                out[i as usize] = HyphenationType::BreakAndReplaceWithHyphen as u8;
            } else {
                out[i as usize] = HyphenationType::DontBreak as u8;
            }
        }
    }

    /// Performs the hyphenation with pattern file.
    fn hyphenate_from_codes(
        &self,
        codes: [u16; MAX_HYPHEN_SIZE as usize],
        len: u32,
        hyphen_value: HyphenationType,
        word: &[u16],
        out: &mut [u8],
    ) {
        let header = Header::new(self.data);
        let trie = header.trie_table();
        let pattern = header.pattern_table();
        let char_mask = trie.char_mask();
        let link_shift = trie.link_shift();
        let link_mask = trie.link_mask();
        let pattern_shift = trie.pattern_shift();
        let max_offset = len - self.min_suffix - 1;

        for i in 0..(len - 1) {
            let mut node: u32 = 0; // index into Trie table
            for j in i..len {
                let c: u32 = codes[j as usize].into();
                let entry = trie.get_at(node + c);
                if (entry & char_mask) == c {
                    node = (entry & link_mask) >> link_shift;
                } else {
                    break;
                }
                let pat_ix = trie.get_at(node) >> pattern_shift;
                // pat_ix contains a 3-tuple of length, shift (number of trailing zeros), and an
                // offset into the buf pool. This is the pattern for the substring (i..j) we just
                // matched, which we combine (via point-wise max) into the buffer vector.
                if pat_ix != 0 {
                    let pat_entry = pattern.entry_at(pat_ix);
                    let pat_len = pat_entry.len();
                    let pat_shift = pat_entry.shift();
                    let offset = j + 1 - (pat_len + pat_shift);
                    // offset is the index within buffer that lines up with the start of pat_buf
                    let start = if self.min_prefix < offset { 0 } else { self.min_prefix - offset };
                    if offset > max_offset {
                        continue;
                    }
                    let end = cmp::min(pat_len, max_offset - offset);
                    for k in start..end {
                        out[(offset + k) as usize] =
                            cmp::max(out[(offset + k) as usize], pat_entry.value_at(k));
                    }
                }
            }
        }

        // Since the above calculation does not modify values outside
        // [mMinPrefix, len - mMinSuffix], they are left as 0 = DONT_BREAK.
        for i in self.min_prefix as usize..max_offset as usize {
            if out[i] & 1 == 0 {
                out[i] = HyphenationType::DontBreak as u8;
                continue;
            }

            if i == 0 || !Self::is_line_breaking_hyphen(word[i - 1]) {
                out[i] = hyphen_value as u8;
                continue;
            }

            if portuguese_hyphenator() {
                if self.locale == HyphenationLocale::Portuguese {
                    // In Portuguese, prefer to break before the hyphen, i.e. the line start with
                    // the hyphen. If we see hyphenation break point after the hyphen character,
                    // prefer to break before the hyphen.
                    out[i - 1] = HyphenationType::BreakAndDontInsertHyphen as u8;
                    out[i] = HyphenationType::DontBreak as u8; // Not prefer to break here because
                                                               // this character is just after the
                                                               // hyphen character.
                } else {
                    // If we see hyphen character just before this character, add hyphenation break
                    // point and don't break here.
                    out[i - 1] = HyphenationType::DontBreak as u8;
                    out[i] = HyphenationType::BreakAndDontInsertHyphen as u8;
                }
            }
        }
    }

    fn hyphenation_type_based_on_script(code_point: u32) -> HyphenationType {
        let script = getScript(code_point);
        if script == USCRIPT_KANNADA
            || script == USCRIPT_MALAYALAM
            || script == USCRIPT_TAMIL
            || script == USCRIPT_TELUGU
        {
            HyphenationType::BreakAndDontInsertHyphen
        } else if script == USCRIPT_ARMENIAN {
            HyphenationType::BreakAndInsertArmenianHyphen
        } else if script == USCRIPT_CANADIAN_ABORIGINAL {
            HyphenationType::BreakAndInsertUcasHyphen
        } else {
            HyphenationType::BreakAndInsertHyphen
        }
    }
}
