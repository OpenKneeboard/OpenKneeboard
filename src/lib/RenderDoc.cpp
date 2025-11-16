// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/RenderDoc.hpp>

#include <Windows.h>

#include <renderdoc_app.h>

namespace OpenKneeboard::RenderDoc {

static HMODULE gModule {NULL};
static RENDERDOC_API_1_3_0* gRenderDoc {nullptr};

static class InitRenderDoc {
 public:
  InitRenderDoc() {
    gModule = GetModuleHandleA("renderdoc.dll");
    if (!gModule) {
      return;
    }
    pRENDERDOC_GetAPI RENDERDOC_GetAPI
      = (pRENDERDOC_GetAPI)GetProcAddress(gModule, "RENDERDOC_GetAPI");
    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_3_0, (void**)&gRenderDoc);
  }
} gInit;

bool IsPresent() {
  InitRenderDoc();
  return gRenderDoc;
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
  if (!(gRenderDoc && gRenderDoc->IsFrameCapturing())) {
    return;
  }
  mRDDevice = rdDevice;

  gRenderDoc->StartFrameCapture(mRDDevice, NULL);
  gRenderDoc->SetCaptureTitle(title);
}

NestedFrameCapture::~NestedFrameCapture() {
  if (mRDDevice) {
    gRenderDoc->EndFrameCapture(mRDDevice, NULL);
  }
}

}// namespace OpenKneeboard::RenderDoc