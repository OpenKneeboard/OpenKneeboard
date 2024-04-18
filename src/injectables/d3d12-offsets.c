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
