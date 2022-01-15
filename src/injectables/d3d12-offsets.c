#include "d3d12-offsets.h"

#include <d3d12.h>

void* VTable_Lookup_ID3D12CommandQueue_ExecuteCommandLists(struct ID3D12CommandQueue* cq) {
  return cq->lpVtbl->ExecuteCommandLists;
}
