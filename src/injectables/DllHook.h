#pragma once

#include <memory>

namespace OpenKneeboard {
class DllHook {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 protected:
  DllHook() = delete;
  DllHook(const char* moduleName);

  /** You probably want to call this from your constructor */
  void InitWithVTable();

  /** Automatically called when the module is available */
  virtual void InstallHook() = 0;

 public:
  virtual ~DllHook();
};
};// namespace OpenKneeboard
