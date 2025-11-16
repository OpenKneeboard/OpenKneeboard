// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include "d3d12-offsets.h"

#include <d3d12.h>

void* VTable_Lookup_ID3D12CommandQueue_ExecuteCommandLists(
  struct ID3D12CommandQueue* cq) {
#ifdef CLANG_TIDY
  return nullptr;
#else
  return cq->lpVtbl->ExecuteCommandLists;
#endif
}
