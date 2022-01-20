#include "ScopedRWX.h"

#include <Windows.h>

namespace OpenKneeboard {

struct ScopedRWX::Impl {
  MEMORY_BASIC_INFORMATION mMBI;
  DWORD mOldProtection;
};

ScopedRWX::ScopedRWX(void* addr) : p(std::make_unique<Impl>()) {
  VirtualQuery(addr, &p->mMBI, sizeof(p->mMBI));
  VirtualProtect(
    p->mMBI.BaseAddress,
    p->mMBI.RegionSize,
    PAGE_EXECUTE_READWRITE,
    &p->mOldProtection);
}

ScopedRWX::~ScopedRWX() {
  DWORD rwx;
  VirtualProtect(
    p->mMBI.BaseAddress, p->mMBI.RegionSize, p->mOldProtection, &rwx);
}

}// namespace OpenKneeboard
