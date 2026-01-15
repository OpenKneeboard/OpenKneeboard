// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "OpenKneeboard/fatal.hpp"

#include <OpenKneeboard/RenderDoc.hpp>

#include <Windows.h>

#include <renderdoc_app.h>

namespace OpenKneeboard::RenderDoc {

struct API {
  HMODULE mModule {NULL};
  RENDERDOC_API_1_3_0* mRenderDoc {nullptr};

  API() {
    mModule = GetModuleHandleA("renderdoc.dll");
    if (!mModule) {
      return;
    }
    const auto getAPI = std::bit_cast<pRENDERDOC_GetAPI>(
      GetProcAddress(mModule, "RENDERDOC_GetAPI"));
    getAPI(eRENDERDOC_API_Version_1_3_0, (void**)&mRenderDoc);
  }

  operator bool() const noexcept {
    return mRenderDoc;
  }

  auto operator->() const noexcept {
    OPENKNEEBOARD_ALWAYS_ASSERT(mRenderDoc);
    return mRenderDoc;
  }

  static const auto& Get() {
    static API sInstance;
    return sInstance;
  }
};

[[nodiscard]]
bool IsPresent() {
  return API::Get();
}

NestedFrameCapture::NestedFrameCapture(
  const VkInstance_T* instance,
  const char* title)
  : NestedFrameCapture(
      RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance),
      title) {
}

NestedFrameCapture::NestedFrameCapture(ID3D12Device* device, const char* title)
  : NestedFrameCapture(static_cast<void*>(device), title) {
}

NestedFrameCapture::NestedFrameCapture(void* rdDevice, const char* title) {
  auto rd = API::Get();
  if (!(rd && rd->IsFrameCapturing())) {
    return;
  }
  mRDDevice = rdDevice;

  rd->StartFrameCapture(mRDDevice, NULL);
  rd->SetCaptureTitle(title);
}

NestedFrameCapture::~NestedFrameCapture() {
  if (mRDDevice) {
    API::Get()->EndFrameCapture(mRDDevice, NULL);
  }
}

}// namespace OpenKneeboard::RenderDoc