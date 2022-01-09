#pragma once

#include <d2d1.h>
#include <winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class D2DErrorRenderer final {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  D2DErrorRenderer(const winrt::com_ptr<ID2D1Factory>& d2df);
  ~D2DErrorRenderer();

  void SetRenderTarget(const winrt::com_ptr<ID2D1RenderTarget>& rt);

  void Render(const std::wstring& text, const D2D1_RECT_F& where);
};

}// namespace OpenKneeboard
