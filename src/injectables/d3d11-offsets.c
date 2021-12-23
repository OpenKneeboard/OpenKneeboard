#include "d3d11-offsets.h"

#include <dxgi.h>

void* VTable_Lookup_IDXGISwapChain_Present(struct IDXGISwapChain* factory) {
  return factory->lpVtbl->Present;
}