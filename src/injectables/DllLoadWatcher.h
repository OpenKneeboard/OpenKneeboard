#pragma once

#include <memory>
#include <string>

namespace OpenKneeboard {

class DllLoadWatcher {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  virtual ~DllLoadWatcher();

 protected:
  DllLoadWatcher(const std::string& name);
  // Call from your constructor
  void InitWithVTable();

  virtual void OnDllLoad(const std::string& name) = 0;
};

}// namespace OpenKneeboard
