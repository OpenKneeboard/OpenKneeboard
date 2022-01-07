#include "OpenKneeboard/D2DErrorRenderer.h"

#include <Unknwn.h>
#include <dwrite.h>

namespace OpenKneeboard {

class D2DErrorRenderer::Impl final {
 public:
  winrt::com_ptr<ID2D1Factory> D2df;
  winrt::com_ptr<IDWriteFactory> Dwf;

  winrt::com_ptr<IDWriteTextFormat> TextFormat;

  winrt::com_ptr<ID2D1RenderTarget> Rt;
  struct {
    winrt::com_ptr<ID2D1Brush> TextBrush;
  } RtVar;
};

D2DErrorRenderer::D2DErrorRenderer(const winrt::com_ptr<ID2D1Factory>& d2df)
  : p(std::make_unique<Impl>()) {
  p->D2df = d2df;
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(p->Dwf.put()));

  p->Dwf->CreateTextFormat(
    L"Segoe UI",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    16.0f,
    L"",
    p->TextFormat.put());
}

void D2DErrorRenderer::SetRenderTarget(
  const winrt::com_ptr<ID2D1RenderTarget>& rt) {
  if (rt == p->Rt) {
    return;
  }

  p->Rt = rt;
  p->RtVar = {};

  rt->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->RtVar.TextBrush.put()));
}

D2DErrorRenderer::~D2DErrorRenderer() {
}

void D2DErrorRenderer::Render(
  const std::wstring& text,
  const D2D1_RECT_F& where) {
  const auto canvasWidth = where.right - where.left,
             canvasHeight = where.bottom - where.top;

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  p->Dwf->CreateTextLayout(
    text.data(),
    static_cast<UINT32>(text.size()),
    p->TextFormat.get(),
    canvasWidth,
    canvasHeight,
    textLayout.put());

  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);

  const auto textWidth = metrics.width, textHeight = metrics.height;

  p->Rt->DrawTextLayout(
    {where.left + ((canvasWidth - textWidth) / 2),
     where.top + ((canvasHeight - textHeight) / 2)},
    textLayout.get(),
    p->RtVar.TextBrush.get());
}

}// namespace OpenKneeboard
