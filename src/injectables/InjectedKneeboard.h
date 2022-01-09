#pragma once

namespace OpenKneeboard {
class InjectedKneeboard {
 protected:
  InjectedKneeboard();

 public:
  virtual ~InjectedKneeboard();

  virtual void Unhook() = 0;
};
};// namespace OpenKneeboard
