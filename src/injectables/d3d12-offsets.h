// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

struct ID3D12CommandQueue;

#ifdef __cplusplus
extern "C" {
#else
#endif

void* VTable_Lookup_ID3D12CommandQueue_ExecuteCommandLists(
  struct ID3D12CommandQueue* p);

#ifdef __cplusplus
}
#endif
