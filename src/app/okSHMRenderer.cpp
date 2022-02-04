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
#include "okSHMRenderer.h"

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <shims/winrt.h>
#include <shims/wx.h>
#include <wincodec.h>

#include "scope_guard.h"

using namespace OpenKneeboard;

namespace {

struct SharedTextureResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<IDXGIKeyedMutex> mMutex;
  winrt::handle mHandle;
  UINT mMutexKey = 0;
};

}// namespace

class okSHMRenderer::Impl {
 public:
  HWND mFeederWindow;
  OpenKneeboard::SHM::Writer mSHM;
  DXResources mDXR;

  winrt::com_ptr<IWICImagingFactory> mWIC;
  winrt::com_ptr<IDWriteFactory> mDWrite;
  // TODO: move to DXResources
  winrt::com_ptr<ID3D11DeviceContext> mD3DContext;
  winrt::com_ptr<ID3D11Texture2D> mCanvasTexture;
  winrt::com_ptr<ID2D1Bitmap1> mCanvasBitmap;
  D2D1_SIZE_U mUsedSize;
  D2D1_RECT_F mClientRect;
  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  std::array<SharedTextureResources, TextureCount> mResources;

  winrt::com_ptr<ID2D1Brush> mErrorBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;
  winrt::com_ptr<ID2D1Brush> mCursorBrush;

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  void SetCanvasSize(const D2D1_SIZE_U& size);
  void RenderError(const wxString& tabTitle, const wxString& message);
  void CopyPixelsToSHM();
  void RenderWithChrome(
    const wxString& tabTitle,
    const D2D1_SIZE_U& preferredContentSize,
    const std::function<void(const D2D1_RECT_F&)>& contentRenderer);

 private:
  void RenderErrorImpl(const wxString& message, const D2D1_RECT_F&);
};

void okSHMRenderer::Impl::SetCanvasSize(const D2D1_SIZE_U& size) {
  mUsedSize = size;
}

void okSHMRenderer::Impl::RenderError(
  const wxString& tabTitle,
  const wxString& message) {
  this->RenderWithChrome(
    tabTitle,
    {768, 1024},
    std::bind_front(&Impl::RenderErrorImpl, this, message));
}

void okSHMRenderer::Impl::RenderErrorImpl(
  const wxString& message,
  const D2D1_RECT_F& rect) {
  auto ctx = mDXR.mD2DDeviceContext;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(rect, mErrorBGBrush.get());

  mErrorRenderer->Render(message.ToStdWstring(), rect);
}

void okSHMRenderer::Impl::CopyPixelsToSHM() {
  if (!mSHM) {
    return;
  }

  mD3DContext->Flush();

  auto& it = mResources.at(mSHM.GetNextTextureIndex());

  it.mMutex->AcquireSync(it.mMutexKey, INFINITE);
  mD3DContext->CopyResource(it.mTexture.get(), mCanvasTexture.get());
  mD3DContext->Flush();
  it.mMutexKey = mSHM.GetNextTextureKey();
  it.mMutex->ReleaseSync(it.mMutexKey);

  SHM::Config config {
    .feederWindow = mFeederWindow,
    .imageWidth = static_cast<uint16_t>(mUsedSize.width),
    .imageHeight = static_cast<uint16_t>(mUsedSize.height),
  };
  mSHM.Update(config);
}

okSHMRenderer::okSHMRenderer(const DXResources& dxr, HWND feederWindow)
  : p(std::make_unique<Impl>()) {
  p->mFeederWindow = feederWindow;
  p->mDXR = dxr;
  p->mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(p->mDWrite.put()));
  p->mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext);

  dxr.mD3DDevice->GetImmediateContext(p->mD3DContext.put());

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = 0,
  };
  winrt::check_hresult(dxr.mD3DDevice->CreateTexture2D(
    &textureDesc, nullptr, p->mCanvasTexture.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    p->mCanvasTexture.as<IDXGISurface>().get(),
    nullptr,
    p->mCanvasBitmap.put()));

  textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
    | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  for (auto i = 0; i < TextureCount; ++i) {
    auto& it = p->mResources.at(i);

    dxr.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, it.mTexture.put());
    auto textureName = SHM::SharedTextureName(i);
    it.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
      nullptr,
      DXGI_SHARED_RESOURCE_READ,
      textureName.c_str(),
      it.mHandle.put());
    it.mMutex = it.mTexture.as<IDXGIKeyedMutex>();
  }

  auto ctx = dxr.mD2DDeviceContext;

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mHeaderBGBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mHeaderTextBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mCursorBrush.put()));

  auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  ctx->CreateSolidColorBrush(
    {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    },
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mErrorBGBrush.put()));
}

okSHMRenderer::~okSHMRenderer() {
  auto ctx = p->mD3DContext;
  p = {};
  ctx->Flush();
}

void okSHMRenderer::SetCursorPosition(float x, float y) {
  p->mCursorPoint = {x, y};
  p->mHaveCursor = true;
}

void okSHMRenderer::HideCursor() {
  p->mHaveCursor = false;
}

void okSHMRenderer::Render(
  const std::shared_ptr<Tab>& tab,
  uint16_t pageIndex) {
  if (!tab) {
    auto msg = _("No Tab");
    p->RenderError(msg, msg);
    return;
  }

  const auto title = tab->GetTitle();
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    p->RenderError(title, _("No Pages"));
    return;
  }

  if (pageIndex >= pageCount) {
    p->RenderError(title, _("Invalid Page Number"));
    return;
  }

  const auto pageSize = tab->GetPreferredPixelSize(pageIndex);
  if (
    pageSize.width == 0 || pageSize.height == 0 || pageSize.width > TextureWidth
    || pageSize.height > TextureHeight) {
    p->RenderError(title, _("Invalid Page Size"));
    return;
  }

  p->RenderWithChrome(
    title, pageSize, std::bind_front(&Tab::RenderPage, tab, pageIndex));
}

void okSHMRenderer::Impl::RenderWithChrome(
  const wxString& tabTitle,
  const D2D1_SIZE_U& pageSize,
  const std::function<void(const D2D1_RECT_F&)>& renderContent) {
  const D2D1_SIZE_U headerSize {
    pageSize.width,
    static_cast<UINT>(pageSize.height * 0.05),
  };
  const D2D1_SIZE_U canvasSize {
    pageSize.width,
    pageSize.height + headerSize.height,
  };
  this->SetCanvasSize(canvasSize);

  mClientRect = {
    .left = 0.0f,
    .top = static_cast<float>(headerSize.height),
    .right = static_cast<float>(canvasSize.width),
    .bottom = static_cast<float>(canvasSize.height),
  };

  auto ctx = mDXR.mD2DDeviceContext;
  ctx->SetTarget(mCanvasBitmap.get());

  ctx->BeginDraw();
  const auto cleanup = scope_guard([&ctx, this]() {
    winrt::check_hresult(ctx->EndDraw());
    this->CopyPixelsToSHM();
  });
  ctx->Clear({0.0f, 0.0f, 0.0f, 0.0f});

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  renderContent(mClientRect);

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(
    {0.0f, 0.0f, float(headerSize.width), float(headerSize.height)},
    mHeaderBGBrush.get());

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);

  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  mDWrite->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put());

  auto title = tabTitle.ToStdWstring();
  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  mDWrite->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put());

  DWRITE_TEXT_METRICS metrics;
  headerLayout->GetMetrics(&metrics);

  ctx->DrawTextLayout(
    {(headerSize.width - metrics.width) / 2,
     (headerSize.height - metrics.height) / 2},
    headerLayout.get(),
    mHeaderTextBrush.get());

  if (!mHaveCursor) {
    return;
  }

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawEllipse(D2D1::Ellipse(mCursorPoint, 2, 2), mCursorBrush.get());
}

D2D1_SIZE_U okSHMRenderer::GetCanvasSize() const {
  return p->mUsedSize;
}

D2D1_RECT_F okSHMRenderer::GetClientRect() const {
  return p->mClientRect;
}
