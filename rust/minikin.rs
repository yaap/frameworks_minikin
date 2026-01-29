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

//! The rust component of libminikin

mod hyphenator;
mod hyphenator_data;

pub use hyphenator::Hyphenator;

use cxx::CxxString;

#[allow(clippy::needless_maybe_sized)]
#[cxx::bridge(namespace = "minikin::rust")]
mod ffi {
    #[namespace = "minikin::rust"]
    unsafe extern "C++" {
        include!("ffi/IcuBridge.h");
        fn getScript(cp: u32) -> u8;
        fn getJoiningType(cp: u32) -> u8;
    }
    #[namespace = "minikin::rust"]
    extern "Rust" {
        type Hyphenator;
        fn load_hyphenator(
            data: &'static [u8],
            min_prefix: u32,
            min_suffix: u32,
            locale: String,
        ) -> Box<Hyphenator>;
        fn load_hyphenator_from_path(
            path: &CxxString,
            min_prefix: u32,
            min_suffix: u32,
            locale: String,
        ) -> Box<Hyphenator>;
        fn hyphenate(hyphenator: &Hyphenator, word: &[u16], out: &mut [u8]);
        fn ensure_initialized(hyphenator: &Hyphenator) -> bool;
    }
}

#[cfg(unix)]
fn load_hyphenator_from_path(
    path: &CxxString,
    min_prefix: u32,
    min_suffix: u32,
    locale: String,
) -> Box<Hyphenator> {
    let path = path.to_str().expect("Path was not valid UTF-8");
    Box::new(Hyphenator::new_from_path(path, min_prefix, min_suffix, &locale))
}

#[cfg(not(unix))]
fn load_hyphenator_from_path(
    _path: &CxxString,
    _min_prefix: u32,
    _min_suffix: u32,
    _locale: String,
) -> Box<Hyphenator> {
    panic!("Not supported")
}

fn load_hyphenator(
    data: &'static [u8],
    min_prefix: u32,
    min_suffix: u32,
    locale: String,
) -> Box<Hyphenator> {
    Box::new(Hyphenator::new(data, min_prefix, min_suffix, &locale))
}

fn hyphenate(hyphenator: &Hyphenator, word: &[u16], out: &mut [u8]) {
    hyphenator.hyphenate(word, out);
}

fn ensure_initialized(hyphenator: &Hyphenator) -> bool {
    hyphenator.ensure_initialized()
}
