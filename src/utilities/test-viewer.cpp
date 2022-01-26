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
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <D2d1.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/SHM.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>
#include <unknwn.h>
#include <winrt/base.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/graphics.h>

#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

using namespace OpenKneeboard;

class Canvas final : public wxWindow {
 private:
  bool mFirstDetached = false;
  SHM::Reader mSHM;
  uint64_t mLastSequenceNumber = 0;

  winrt::com_ptr<ID3D11Device> mD3d;
  winrt::com_ptr<ID3D11DeviceContext> mD3dContext;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  winrt::com_ptr<ID2D1Factory> mD2df;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;
  winrt::com_ptr<IDXGIFactory2> mDXGI;

 public:
  Canvas(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, {768 / 2, 1024 / 2}) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, mD2df.put());
    UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    auto d3dLevel = D3D_FEATURE_LEVEL_11_0;
    D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      d3dFlags,
      &d3dLevel,
      1,
      D3D11_SDK_VERSION,
      mD3d.put(),
      nullptr,
      mD3dContext.put());
    CreateDXGIFactory(IID_PPV_ARGS(mDXGI.put()));

    mErrorRenderer = std::make_unique<D2DErrorRenderer>(mD2df);

    Bind(wxEVT_PAINT, &Canvas::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, [this](auto&) {});
    Bind(wxEVT_SIZE, &Canvas::OnSize, this);
  }

  void OnSize(wxSizeEvent& ev) {
    Refresh();
    Update();
  }

  void CheckForUpdate() {
    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      if (mFirstDetached) {
        Refresh();
        Update();
      }
      return;
    }

    if (snapshot.GetSequenceNumber() != mLastSequenceNumber) {
      Refresh();
      Update();
    }
  }

  void initSwapChain() {
    auto desiredSize = this->GetClientSize();
    if (mSwapChain) {
      DXGI_SWAP_CHAIN_DESC desc;
      mSwapChain->GetDesc(&desc);
      auto& mode = desc.BufferDesc;
      if (
        mode.Width == desiredSize.GetWidth()
        && mode.Height == desiredSize.GetHeight()) {
        return;
      }
      mBackgroundBrush = nullptr;
      mErrorRenderer->SetRenderTarget(nullptr);
      mD3dContext->ClearState();
      mSwapChain->ResizeBuffers(
        desc.BufferCount,
        desiredSize.x,
        desiredSize.y,
        mode.Format,
        desc.Flags);
      return;
    }

    static_assert(SHM::Pixel::IS_PREMULTIPLIED_B8G8R8A8);
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Width = static_cast<UINT>(desiredSize.GetWidth()),
      .Height = static_cast<UINT>(desiredSize.GetHeight()),
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };
    mDXGI->CreateSwapChainForHwnd(
      mD3d.get(),
      this->GetHWND(),
      &swapChainDesc,
      nullptr,
      nullptr,
      mSwapChain.put());
  }

  void OnPaint(wxPaintEvent& ev) {
    this->initSwapChain();
    const auto clientSize = GetClientSize();

    winrt::com_ptr<IDXGISurface> surface;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
    winrt::com_ptr<ID2D1RenderTarget> rt;
    auto ret = mD2df->CreateDxgiSurfaceRenderTarget(
      surface.get(),
      {.type = D2D1_RENDER_TARGET_TYPE_HARDWARE,
       .pixelFormat = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE}},
      rt.put());

    mErrorRenderer->SetRenderTarget(rt);

    if (!mBackgroundBrush) {
      winrt::com_ptr<ID2D1Bitmap> backgroundBitmap;
      SHM::Pixel pixels[20 * 20];
      for (int x = 0; x < 20; x++) {
        for (int y = 0; y < 20; y++) {
          bool white = (x < 10 && y < 10) || (x >= 10 && y >= 10);
          uint8_t value = white ? 0xff : 0xcc;
          pixels[x + (20 * y)] = {value, value, value, 0xff};
        }
      }
      rt->CreateBitmap(
        {20, 20},
        reinterpret_cast<BYTE*>(pixels),
        20 * sizeof(SHM::Pixel),
        D2D1::BitmapProperties(D2D1::PixelFormat(
          DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        backgroundBitmap.put());

      mBackgroundBrush = nullptr;
      rt->CreateBitmapBrush(
        backgroundBitmap.get(),
        D2D1::BitmapBrushProperties(
          D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP),
        reinterpret_cast<ID2D1BitmapBrush**>(mBackgroundBrush.put()));
    }

    wxPaintDC dc(this);
    rt->BeginDraw();
    wxON_BLOCK_EXIT0([&]() {
      rt->EndDraw();
      mSwapChain->Present(0, 0);
    });

    auto wxBgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    rt->Clear(
      {wxBgColor.Red() / 255.0f,
       wxBgColor.Green() / 255.0f,
       wxBgColor.Blue() / 255.0f,
       1.0f});

    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      mErrorRenderer->Render(
        _("No Feeder").ToStdWstring(),
        {0.0f,
         0.0f,
         float(clientSize.GetWidth()),
         float(clientSize.GetHeight())});
      mFirstDetached = false;
      return;
    }
    mFirstDetached = true;

    const auto& config = *snapshot.GetConfig();

    if (config.imageWidth == 0 || config.imageHeight == 0) {
      mErrorRenderer->Render(
        _("Invalid Image").ToStdWstring(),
        {0.0f,
         0.0f,
         float(clientSize.GetWidth()),
         float(clientSize.GetHeight())});
    }

    wxBgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWFRAME);
    rt->Clear(
      {wxBgColor.Red() / 255.0f,
       wxBgColor.Green() / 255.0f,
       wxBgColor.Blue() / 255.0f,
       1.0f});

    const auto scalex = float(clientSize.GetWidth()) / config.imageWidth;
    const auto scaley = float(clientSize.GetHeight()) / config.imageHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = config.imageWidth * scale;
    const auto renderHeight = config.imageHeight * scale;

    const auto renderLeft = (clientSize.GetWidth() - renderWidth) / 2;
    const auto renderTop = (clientSize.GetHeight() - renderHeight) / 2;
    auto dpi = GetDpiForWindow(this->GetHWND());
    D2D1_RECT_F pageRect {
      renderLeft * (dpi / 96.0f),
      renderTop * (dpi / 96.0f),
      (renderLeft + renderWidth) * (dpi / 96.0f),
      (renderTop + renderHeight) * (dpi / 96.0f)};

    winrt::com_ptr<IDXGISurface> sharedSurface;
    mD3d.as<ID3D11Device1>()->OpenSharedResourceByName(
      L"Local\\OpenKneeboard",
      DXGI_SHARED_RESOURCE_READ,
      IID_PPV_ARGS(sharedSurface.put()));
    winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
    static_assert(SHM::Pixel::IS_PREMULTIPLIED_B8G8R8A8);
    D2D1_PIXEL_FORMAT pixelFormat {
      DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED};
    D2D1_BITMAP_PROPERTIES bitmapProperties {
      .pixelFormat = pixelFormat,
      .dpiX = static_cast<FLOAT>(dpi),
      .dpiY = static_cast<FLOAT>(dpi),
    };
    auto csbRet = rt->CreateSharedBitmap(
      _uuidof(IDXGISurface),
      sharedSurface.get(),
      &bitmapProperties,
      d2dBitmap.put());

    auto bg = mBackgroundBrush;
    // Align the top-left pixel of the brush
    bg->SetTransform(
      D2D1::Matrix3x2F::Translation({pageRect.left, pageRect.top}));
    rt->FillRectangle(pageRect, bg.get());
    rt->SetTransform(D2D1::IdentityMatrix());
    rt->DrawBitmap(
      d2dBitmap.get(), &pageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    mLastSequenceNumber = snapshot.GetSequenceNumber();
  }
};

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;

 public:
  MainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard Test Viewer") {
    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      fileMenu->Append(wxID_EXIT, _T("E&xit"));
      menuBar->Append(fileMenu, _("&File"));
    }
    SetMenuBar(menuBar);

    auto canvas = new Canvas(this);

    mTimer.Bind(wxEVT_TIMER, [canvas](auto) { canvas->CheckForUpdate(); });
    mTimer.Start(100);

    Bind(
      wxEVT_MENU, [this](auto&) { this->Close(true); }, wxID_EXIT);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(canvas, 1, wxEXPAND);
    this->SetSizerAndFit(sizer);
  }

  void OnExit(wxCommandEvent& ev) {
    Close(true);
  }
};

class TestViewApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    wxInitAllImageHandlers();
    MainWindow* window = new MainWindow();
    window->Show();
    return true;
  }
};

wxIMPLEMENT_APP(TestViewApp);
