#include "dxgi-offsets.h"

#include <dxgi.h>

void* VTable_Lookup_IDXGISwapChain_Present(struct IDXGISwapChain* factory) {
  return factory->lpVtbl->Present;
}
