#include "okSHMRenderer.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <Unknwn.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <winrt/base.h>

#include "OpenKneeboard/D2DErrorRenderer.h"
#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/Tab.h"
#include "OpenKneeboard/dprint.h"

using namespace OpenKneeboard;

class okSHMRenderer::Impl {
 public:
  OpenKneeboard::SHM::Writer mSHM;

  winrt::com_ptr<IWICImagingFactory> mWIC;
  winrt::com_ptr<ID2D1Factory> mD2D;
  winrt::com_ptr<IDWriteFactory> mDWrite;
  winrt::com_ptr<IWICBitmap> mCanvas;
  winrt::com_ptr<ID2D1RenderTarget> mRt;
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  void SetCanvasSize(const D2D1_SIZE_U& size);
  void RenderError(const wxString& error);
  void CopyPixelsToSHM();
};

void okSHMRenderer::Impl::SetCanvasSize(const D2D1_SIZE_U& size) {
  if (this->mCanvas) {
    UINT width, height;
    this->mCanvas->GetSize(&width, &height);
    if (width == size.width && height == size.height) {
      return;
    }
    this->mCanvas = nullptr;
  }

  this->mWIC->CreateBitmap(
    size.width,
    size.height,
    GUID_WICPixelFormat32bppPBGRA,
    WICBitmapCacheOnDemand,
    this->mCanvas.put());

  this->mRt = nullptr;
  this->mHeaderBGBrush = nullptr;
  this->mHeaderTextBrush = nullptr;

  this->mD2D->CreateWicBitmapRenderTarget(
    this->mCanvas.get(),
    D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1_PIXEL_FORMAT {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED,
      }),
    this->mRt.put());

  this->mErrorRenderer->SetRenderTarget(this->mRt);

  this->mRt->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(this->mHeaderBGBrush.put()));
  this->mRt->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(this->mHeaderTextBrush.put()));
}

void okSHMRenderer::Impl::RenderError(const wxString& message) {
  if (!this->mCanvas) {
    this->SetCanvasSize({768, 1024});
  }

  UINT width, height;
  this->mCanvas->GetSize(&width, &height);

  auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

  this->mRt->BeginDraw();
  this->mRt->SetTransform(D2D1::Matrix3x2F::Identity());

  this->mRt->Clear(D2D1_COLOR_F {
    bg.Red() / 255.0f,
    bg.Green() / 255.0f,
    bg.Blue() / 255.0f,
    bg.Alpha() / 255.0f,
  });

  this->mErrorRenderer->Render(
    message.ToStdWstring(), {0.0f, 0.0f, float(width), float(height)});

  this->mRt->EndDraw();
  this->CopyPixelsToSHM();
}

void okSHMRenderer::Impl::CopyPixelsToSHM() {
  if (!this->mSHM) {
    return;
  }

  UINT width, height;
  this->mCanvas->GetSize(&width, &height);
  if (width == 0 || height == 0) {
    return;
  }

  const auto ratio = float(width) / height;

  SHM::Header header {
    .Flags = SHM::Flags::DISCARD_DEPTH_INFORMATION,
    .x = 0.15f,
    .floorY = 0.6f,
    .eyeY = -0.7f,
    .z = -0.4f,
    .rx = -float(2 * M_PI / 5),
    .rz = -float(M_PI / 32),
    .VirtualWidth = 0.25f / ratio,
    .VirtualHeight = 0.25f,
    .ZoomScale = 2.0f,
    .ImageWidth = static_cast<uint16_t>(width),
    .ImageHeight = static_cast<uint16_t>(height),
  };

  using Pixel = SHM::Pixel;
  static_assert(sizeof(Pixel) == 4, "Expecting B8G8R8A8 for SHM");
  static_assert(offsetof(Pixel, b) == 0, "Expected blue to be first byte");
  static_assert(offsetof(Pixel, a) == 3, "Expected alpha to be last byte");

  std::vector<Pixel> pixels(width * height);
  this->mCanvas->CopyPixels(
    nullptr,
    width * sizeof(Pixel),
    pixels.size() * sizeof(Pixel),
    reinterpret_cast<BYTE*>(pixels.data()));

  this->mSHM.Update(header, pixels);
}

okSHMRenderer::okSHMRenderer() : p(std::make_unique<Impl>()) {
  p->mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
  D2D1CreateFactory(
    D2D1_FACTORY_TYPE_SINGLE_THREADED,
    __uuidof(ID2D1Factory),
    p->mD2D.put_void());
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(p->mDWrite.put()));
  p->mErrorRenderer = std::make_unique<D2DErrorRenderer>(p->mD2D);
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

  p->mRt->BeginDraw();
  wxON_BLOCK_EXIT0([this]() {
    this->p->mRt->EndDraw();
    this->p->CopyPixelsToSHM();
  });
  p->mRt->SetTransform(D2D1::Matrix3x2F::Identity());
  p->mRt->Clear({1.0f, 0.0f, 1.0f, 1.0f});

  tab->RenderPage(
    pageIndex,
    p->mRt,
    {
      .left = 0.0f,
      .top = float(headerSize.height),
      .right = float(canvasSize.width),
      .bottom = float(canvasSize.height),
    });

  p->mRt->FillRectangle(
    {0.0f, 0.0f, float(headerSize.width), float(headerSize.height)},
    p->mHeaderBGBrush.get());

  FLOAT dpix, dpiy;
  p->mRt->GetDpi(&dpix, &dpiy);

  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  p->mDWrite->CreateTextFormat(
    L"Courier New",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put());

  auto title = wxString::FromUTF8(tab->GetTitle()).ToStdWstring();
  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  p->mDWrite->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put());

  DWRITE_TEXT_METRICS metrics;
  headerLayout->GetMetrics(&metrics);

  p->mRt->DrawTextLayout(
    {(headerSize.width - metrics.width) / 2,
     (headerSize.height - metrics.height) / 2},
    headerLayout.get(),
    p->mHeaderTextBrush.get());
}
