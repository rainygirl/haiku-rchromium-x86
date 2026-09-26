// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// memory_stubs.cc without UncheckedCalloc.
//
// The stubs file is written for platforms that build their own
// process/memory.cc -- iOS and NaCl both take the generic one out of their
// source list. Haiku keeps it, because it is also where
// internal::ReleaseAddressSpaceReservation() lives, and then the two files
// define UncheckedCalloc twice. The linker says so, at the very end, after
// everything else has compiled.
//
// So: the stubs minus that one function. The generic memory.cc's version is
// the better of the two anyway -- it checks num_items * size for overflow,
// where calloc() on this libc is trusted to do it.

#include "base/process/memory.h"

#include <stddef.h>
#include <stdlib.h>

namespace base {

// Haiku has no way to make an allocation failure terminate the process, so
// these are empty and UncheckedMalloc below is a plain malloc -- which is
// exactly the arrangement the stubs file describes.
void EnableTerminationOnOutOfMemory() {
}

void EnableTerminationOnHeapCorruption() {
}

bool AdjustOOMScore(ProcessId process, int score) {
  return false;
}

bool UncheckedMalloc(size_t size, void** result) {
  *result = malloc(size);
  return *result != nullptr;
}

}  // namespace base
