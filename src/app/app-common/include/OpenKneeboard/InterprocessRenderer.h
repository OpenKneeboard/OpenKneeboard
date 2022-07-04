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
#pragma once

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/config.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <shims/winrt.h>

#include <memory>
#include <mutex>
#include <optional>

namespace OpenKneeboard {
class CursorEvent;
class CursorRenderer;
class KneeboardState;
class ITab;
class TabAction;
struct DXResources;

class InterprocessRenderer final : private EventReceiver {
 public:
  InterprocessRenderer() = delete;
  InterprocessRenderer(const DXResources&, KneeboardState*);
  ~InterprocessRenderer();

 private:
  winrt::apartment_context mUIThread;
  OpenKneeboard::SHM::Writer mSHM;
  DXResources mDXR;

  KneeboardState* mKneeboard = nullptr;

  bool mNeedsRepaint = true;

  // TODO: move to DXResources
  winrt::com_ptr<ID3D11DeviceContext> mD3DContext;

  struct SharedTextureResources {
    winrt::com_ptr<ID3D11Texture2D> mTexture;
    winrt::com_ptr<IDXGIKeyedMutex> mMutex;
    winrt::handle mHandle;
    UINT mMutexKey = 0;
  };

  struct Layer {
    SHM::LayerConfig mConfig;
    std::shared_ptr<IKneeboardView> mKneeboardView;

    winrt::com_ptr<ID3D11Texture2D> mCanvasTexture;
    winrt::com_ptr<ID2D1Bitmap1> mCanvasBitmap;

    std::array<SharedTextureResources, TextureCount> mSharedResources;

    bool mIsActiveForInput = false;
  };
  std::array<Layer, MaxLayers> mLayers;

  struct Button;
  class Toolbar;
  std::unordered_map<KneeboardViewID, std::shared_ptr<Toolbar>> mToolbars;

  winrt::com_ptr<ID2D1Brush> mErrorBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;
  winrt::com_ptr<ID2D1Brush> mButtonBrush;
  winrt::com_ptr<ID2D1Brush> mDisabledButtonBrush;
  winrt::com_ptr<ID2D1Brush> mHoverButtonBrush;
  winrt::com_ptr<ID2D1Brush> mActiveButtonBrush;

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  std::unique_ptr<CursorRenderer> mCursorRenderer;

  void RenderNow();
  void Render(Layer&);
  void RenderError(Layer&, utf8_string_view tabTitle, utf8_string_view message);
  void RenderErrorImpl(utf8_string_view message, const D2D1_RECT_F&);
  void RenderWithChrome(
    Layer&,
    const std::string_view tabTitle,
    const D2D1_SIZE_U& preferredContentSize,
    const std::function<void(const D2D1_RECT_F&)>& contentRenderer);
  void RenderToolbar(Layer&);

  void Commit(uint8_t layerCount);

  void OnLayoutChanged(const std::weak_ptr<IKneeboardView>&);
  void OnCursorEvent(const std::weak_ptr<IKneeboardView>&, const CursorEvent&);
};

}// namespace OpenKneeboard
