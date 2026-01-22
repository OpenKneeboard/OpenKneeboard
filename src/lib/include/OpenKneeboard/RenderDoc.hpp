// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

struct ID3D12Device;
struct VkInstance_T;

namespace OpenKneeboard::RenderDoc {

bool IsPresent();

class NestedFrameCapture final {
 public:
  NestedFrameCapture() = delete;
  NestedFrameCapture(const VkInstance_T*, const char* title);
  NestedFrameCapture(ID3D12Device*, const char* title);
  ~NestedFrameCapture();

 private:
  explicit NestedFrameCapture(void* rdDevice, const char* title);
  void* mRDDevice {nullptr};
};

};// namespace OpenKneeboard::RenderDoc
