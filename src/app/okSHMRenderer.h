#pragma once

#include <memory>

namespace OpenKneeboard {
class Tab;
}

class okSHMRenderer final {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  okSHMRenderer();
  ~okSHMRenderer();

  void Render(
    const std::shared_ptr<OpenKneeboard::Tab>& tab,
    uint16_t pageIndex);
};