#pragma once

#include <memory>
#include <string>

namespace OpenKneeboard {

class DllLoadWatcher {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  DllLoadWatcher(const std::string& name);
  virtual ~DllLoadWatcher();

 protected:
  virtual void OnDllLoad(const std::string& name) = 0;
};

}// namespace OpenKneeboard
