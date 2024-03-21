#include "dxgi-offsets.h"

#include <dxgi.h>

void* VTable_Lookup_IDXGISwapChain_Present(struct IDXGISwapChain* swapchain) {
  return swapchain->lpVtbl->Present;
}

void* VTable_Lookup_IDXGISwapChain_ResizeBuffers(
  struct IDXGISwapChain* swapchain) {
  return swapchain->lpVtbl->ResizeBuffers;
}
