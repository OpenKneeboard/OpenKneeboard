// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ChromiumPageSource.hpp>

#include <include/cef_base.h>

namespace OpenKneeboard {
class ChromiumPageSource::RenderHandler final : public CefRenderHandler {
 public:
  struct Frame {
    PixelSize mSize {};
    wil::com_ptr<ID3D11Texture2D> mTexture;
    wil::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
  };

  uint64_t mFrameCount = 0;
  wil::com_ptr<ID3D11Fence> mFence;
  std::array<Frame, 3> mFrames;

  RenderHandler() = delete;
  RenderHandler(std::shared_ptr<ChromiumPageSource> pageSource);
  ~RenderHandler();

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override;

  void OnPaint(
    CefRefPtr<CefBrowser>,
    PaintElementType,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) override;

  void OnAcceleratedPaint(
    CefRefPtr<CefBrowser>,
    PaintElementType,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) override;

  PixelSize GetSize() const;
  void SetSize(const PixelSize& size);

  void RenderPage(RenderContext rc, const PixelRect& rect);

 private:
  IMPLEMENT_REFCOUNTING(RenderHandler);

  std::weak_ptr<ChromiumPageSource> mPageSource;
  PixelSize mSize {};
};

}// namespace OpenKneeboard
