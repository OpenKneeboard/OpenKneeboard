/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/RenderDoc.h>

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