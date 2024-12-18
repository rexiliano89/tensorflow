/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <string_view>

#include "absl/container/flat_hash_set.h"
#include "third_party/gpus/cuda/include/cuda.h"
#include "third_party/gpus/cuda/include/nvJitLink.h"
#include "tsl/platform/dso_loader.h"
#include "tsl/platform/load_library.h"
#include "tsl/platform/logging.h"

// Implements the NvJitLink API by forwarding to NvJitLink loaded from the DSO.

namespace {
// Returns DSO handle or null if loading the DSO fails.
void* GetDsoHandle() {
#ifdef PLATFORM_GOOGLE
  return nullptr;
#else
  static auto handle = []() -> void* {
    auto handle_or = tsl::internal::DsoLoader::GetCusparseDsoHandle();
    if (!handle_or.ok()) return nullptr;
    return handle_or.value();
  }();
  return handle;
#endif
}

void* LoadSymbol(const char* symbol_name) {
  void* symbol = nullptr;
  if (auto handle = GetDsoHandle()) {
    tsl::internal::GetSymbolFromLibrary(handle, symbol_name, &symbol)
        .IgnoreError();
  }
  return symbol;
}

const char* kSymbols[] = {
#include "xla/tsl/cuda/nvjitlink.inc"
};

constexpr size_t kNumSymbols = sizeof(kSymbols) / sizeof(const char*);
}  // namespace

extern "C" {

static nvJitLinkResult GetSymbolNotFoundError() {
  return NVJITLINK_ERROR_INTERNAL;
}

extern void* _nvjitlink_tramp_table[];

void _nvjitlink_tramp_resolve(int i) {
  CHECK_LE(0, i);
  CHECK_LT(i, kNumSymbols);
  void* p = LoadSymbol(kSymbols[i]);
  if (!p) {
    p = reinterpret_cast<void*>(&GetSymbolNotFoundError);
  }
  _nvjitlink_tramp_table[i] = p;
}
}
