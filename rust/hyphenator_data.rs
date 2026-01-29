/*
 * Copyright 2026 The Android Open Source Project
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

#[cfg(unix)]
use std::fs::File;
#[cfg(unix)]
use std::os::fd::AsRawFd;
#[cfg(unix)]
use std::path::PathBuf;
use std::sync::OnceLock;

#[cfg(unix)]
use log::error;

pub struct HyphenatorData {
    #[cfg(unix)]
    path: Option<PathBuf>,
    data: OnceLock<Option<&'static [u8]>>,
}

impl HyphenatorData {
    #[cfg(unix)]
    pub fn new_from_path(path: PathBuf) -> Self {
        Self { path: Some(path), data: OnceLock::new() }
    }

    pub fn new_from_ptr(ptr: &'static [u8]) -> Self {
        let data = OnceLock::new();
        data.set(Some(ptr)).expect("OnceLock somehow already initialized");
        Self {
            #[cfg(unix)]
            path: None,
            data,
        }
    }

    pub fn get(&self) -> &'static [u8] {
        self.data
            .get()
            .expect("get() called on uninitialized data")
            .expect("get() called on uninitialized data")
    }

    #[cfg(unix)]
    pub fn ensure_initialized(&self) -> bool {
        self.data
            .get_or_init(|| {
                let Some(path) = self.path.as_ref() else {
                    error!("Tried to use hyphenator with no file or data");
                    return None;
                };

                let file = match File::open(path) {
                    Ok(file) => file,
                    Err(err) => {
                        error!("Failed to open path {}", err);
                        return None;
                    }
                };

                let len = match file.metadata() {
                    Ok(metadata) => metadata.len() as usize,
                    Err(err) => {
                        error!("Failed to get .hyb file length {}", err);
                        return None;
                    }
                };
                let nullptr = std::ptr::null_mut();
                // SAFETY: The mmap syscall doesn't modify any rust controlled memory.
                let ptr = unsafe {
                    libc::mmap(nullptr, len, libc::PROT_READ, libc::MAP_SHARED, file.as_raw_fd(), 0)
                };
                if ptr == libc::MAP_FAILED {
                    error!("Failed to mmap .hyb file");
                    return None;
                }
                let ptr = ptr as *const u8;
                // SAFETY: ptr is non-null and never unmapped. It is at least len bytes long,
                // and u8 has no alignment requriements. The mapped file is read-only, so the
                // memory will never be mutated.
                Some(unsafe { std::slice::from_raw_parts(ptr, len) })
            })
            .is_some()
    }

    #[cfg(not(unix))]
    pub fn ensure_initialized(&self) -> bool {
        true
    }
}
