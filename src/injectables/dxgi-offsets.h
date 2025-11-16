// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

struct IDXGISwapChain;

#ifdef __cplusplus
extern "C" {
#else
#endif

void* VTable_Lookup_IDXGISwapChain_Present(struct IDXGISwapChain* p);
void* VTable_Lookup_IDXGISwapChain_ResizeBuffers(struct IDXGISwapChain* p);

#ifdef __cplusplus
}
#endif
