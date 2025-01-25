/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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

#include <OpenKneeboard/ChromiumPageSource.hpp>

#include <include/cef_base.h>

namespace OpenKneeboard {
class ChromiumPageSource::RenderHandler final : public CefRenderHandler {
 public:
  uint64_t mFrameCount = 0;
  wil::com_ptr<ID3D11Fence> mFence;
  std::array<Frame, 3> mFrames;

  RenderHandler() = delete;
  RenderHandler(ChromiumPageSource* pageSource);
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
    const CefAcceleratedPaintInfo& info);

  void SetSize(const PixelSize& size);

  void RenderPage(RenderContext rc, const PixelRect& rect);

 private:
  IMPLEMENT_REFCOUNTING(RenderHandler);

  ChromiumPageSource* mPageSource {nullptr};
  PixelSize mSize {};
};

}// namespace OpenKneeboard