#pragma once
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ThreadGuard.h>

#include <OpenKneeboard/inttypes.h>

#include <unordered_set>

#include <imgui.h>

namespace OpenKneeboard {
class KneeboardState;

class ImguiInstance final {
 public:
  ImguiInstance(const DXResources&, KneeboardState*);
  ~ImguiInstance();
  void Render(ID2D1DeviceContext*, PageID, const D2D1_RECT_F& targetRect);

 private:
  DXResources mDXR;
  KneeboardState* mKneeboard;
  ImGuiContext* mImguiCtx;

  winrt::com_ptr<ID2D1DeviceContext> m2DContext;
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID2D1Bitmap1> mBitmap;
};

}// namespace OpenKneeboard