#include "okSHMRenderer.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/dprint.h>
#include <Unknwn.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <winrt/base.h>

using namespace OpenKneeboard;

class okSHMRenderer::Impl {
 public:
  OpenKneeboard::SHM::Writer shm;

  winrt::com_ptr<IWICImagingFactory> wic;
  winrt::com_ptr<ID2D1Factory> d2d;
  winrt::com_ptr<IDWriteFactory> dwrite;
  winrt::com_ptr<IWICBitmap> canvas;
  winrt::com_ptr<ID2D1RenderTarget> rt;
  winrt::com_ptr<ID2D1Brush> headerBGBrush;
  winrt::com_ptr<ID2D1Brush> headerTextBrush;

  std::unique_ptr<D2DErrorRenderer> errorRenderer;

  void SetCanvasSize(const D2D1_SIZE_U& size);
  void RenderError(const wxString& error);
  void CopyPixelsToSHM();
};

void okSHMRenderer::Impl::SetCanvasSize(const D2D1_SIZE_U& size) {
  if (this->canvas) {
    UINT width, height;
    this->canvas->GetSize(&width, &height);
    if (width == size.width && height == size.height) {
      return;
    }
    this->canvas = nullptr;
  }

  this->wic->CreateBitmap(
    size.width,
    size.height,
    GUID_WICPixelFormat32bppPBGRA,
    WICBitmapCacheOnDemand,
    this->canvas.put());

  this->rt = nullptr;
  this->headerBGBrush = nullptr;
  this->headerTextBrush = nullptr;

  this->d2d->CreateWicBitmapRenderTarget(
    this->canvas.get(),
    D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1_PIXEL_FORMAT {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED,
      }),
    this->rt.put());

  this->errorRenderer->SetRenderTarget(this->rt);

  this->rt->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(this->headerBGBrush.put()));
  this->rt->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(this->headerTextBrush.put()));
}

void okSHMRenderer::Impl::RenderError(const wxString& message) {
  if (!this->canvas) {
    this->SetCanvasSize({768, 1024});
  }

  UINT width, height;
  this->canvas->GetSize(&width, &height);

  auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

  this->rt->BeginDraw();
  this->rt->SetTransform(D2D1::Matrix3x2F::Identity());

  this->rt->Clear(D2D1_COLOR_F {
    bg.Red() / 255.0f,
    bg.Green() / 255.0f,
    bg.Blue() / 255.0f,
    bg.Alpha() / 255.0f,
  });

  this->errorRenderer->Render(
    message.ToStdWstring(), {0.0f, 0.0f, float(width), float(height)});

  this->rt->EndDraw();
  this->CopyPixelsToSHM();
}

void okSHMRenderer::Impl::CopyPixelsToSHM() {
  if (!this->shm) {
    return;
  }

  UINT width, height;
  this->canvas->GetSize(&width, &height);
  if (width == 0 || height == 0) {
    return;
  }

  SHM::Config config {
    .imageWidth = static_cast<uint16_t>(width),
    .imageHeight = static_cast<uint16_t>(height),
    .vr = { .flags = SHM::VRConfig::Flags::DISCARD_DEPTH_INFORMATION },
  };

  using Pixel = SHM::Pixel;
  static_assert(Pixel::IS_PREMULTIPLIED_B8G8R8A8);

  std::vector<Pixel> pixels(width * height);
  this->canvas->CopyPixels(
    nullptr,
    width * sizeof(Pixel),
    pixels.size() * sizeof(Pixel),
    reinterpret_cast<BYTE*>(pixels.data()));

  this->shm.Update(config, pixels);
}

okSHMRenderer::okSHMRenderer() : p(std::make_unique<Impl>()) {
  p->wic = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, p->d2d.put());
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(p->dwrite.put()));
  p->errorRenderer = std::make_unique<D2DErrorRenderer>(p->d2d);
}

okSHMRenderer::~okSHMRenderer() {
}

void okSHMRenderer::Render(
  const std::shared_ptr<Tab>& tab,
  uint16_t pageIndex) {
  if (!tab) {
    p->RenderError(_("No Kneeboard Tab"));
    return;
  }

  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    p->RenderError(_("No Pages"));
    return;
  }

  if (pageIndex >= pageCount) {
    p->RenderError(_("Invalid Page Number"));
    return;
  }

  const auto pageSize = tab->GetPreferredPixelSize(pageIndex);
  if (pageSize.width == 0 || pageSize.height == 0) {
    p->RenderError(_("Invalid Page Size"));
    return;
  }

  const D2D1_SIZE_U headerSize {
    pageSize.width,
    static_cast<UINT>(pageSize.height * 0.05),
  };
  const D2D1_SIZE_U canvasSize {
    pageSize.width,
    pageSize.height + headerSize.height,
  };
  p->SetCanvasSize(canvasSize);

  p->rt->BeginDraw();
  wxON_BLOCK_EXIT0([this]() {
    this->p->rt->EndDraw();
    this->p->CopyPixelsToSHM();
  });
  p->rt->Clear({0.0f, 0.0f, 0.0f, 0.0f});

  p->rt->SetTransform(D2D1::Matrix3x2F::Identity());
  tab->RenderPage(
    pageIndex,
    p->rt,
    {
      .left = 0.0f,
      .top = float(headerSize.height),
      .right = float(canvasSize.width),
      .bottom = float(canvasSize.height),
    });

  p->rt->SetTransform(D2D1::Matrix3x2F::Identity());
  p->rt->FillRectangle(
    {0.0f, 0.0f, float(headerSize.width), float(headerSize.height)},
    p->headerBGBrush.get());

  FLOAT dpix, dpiy;
  p->rt->GetDpi(&dpix, &dpiy);

  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  p->dwrite->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put());

  auto title = wxString::FromUTF8(tab->GetTitle()).ToStdWstring();
  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  p->dwrite->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put());

  DWRITE_TEXT_METRICS metrics;
  headerLayout->GetMetrics(&metrics);

  p->rt->DrawTextLayout(
    {(headerSize.width - metrics.width) / 2,
     (headerSize.height - metrics.height) / 2},
    headerLayout.get(),
    p->headerTextBrush.get());
}
