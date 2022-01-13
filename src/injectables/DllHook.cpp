#include "DllHook.h"

#include <OpenKneeboard/dprint.h>
#include <Windows.h>

#include "DllLoadWatcher.h"

namespace OpenKneeboard {

struct DllHook::Impl final : public DllLoadWatcher {
  Impl(DllHook* hook, const char* moduleName);
  DllHook* mHook = nullptr;
  std::string mName;

 protected:
  virtual void OnDllLoad(const std::string& name) override;
};

DllHook::Impl::Impl(DllHook* hook, const char* name)
  : DllLoadWatcher(name), mHook(hook), mName(name) {
}

void DllHook::Impl::OnDllLoad(const std::string& name) {
  dprintf("DLL '{}' loaded, installing hook", name);
  mHook->InstallHook();
}

DllHook::DllHook(const char* name)
  : p(std::make_unique<Impl>(this, name)) {
}

void DllHook::InitWithVTable() {
  if (!GetModuleHandleA(p->mName.c_str())) {
    dprintf("DLL '{}' not yet loaded, may install later", p->mName);
    return;
  }
  dprintf("Installing hook for '{}'", p->mName);
  this->InstallHook();
}

DllHook::~DllHook() {
}

}// namespace OpenKneeboard
