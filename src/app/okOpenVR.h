#pragma once

#include <memory>
#include <vector>

namespace OpenKneeboard::SHM {
struct Header;
struct Pixel;
}// namespace OpenKneeboard::SHM

class okOpenVR final {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  okOpenVR();
  ~okOpenVR();

  void Update(
    const OpenKneeboard::SHM::Header& header,
    OpenKneeboard::SHM::Pixel* pixels);
};
