#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <D2d1.h>
#include <unknwn.h>
#include <wincodec.h>
#include <winrt/base.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/graphics.h>

#include "OpenKneeboard/RenderError.h"
#include "OpenKneeboard/SHM.h"

#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Windowscodecs.lib")

using namespace OpenKneeboard;

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;
  bool mFirstDetached = false;
  bool mHadData = false;
  SHM::Reader mSHM;
  uint64_t mLastSequenceNumber = 0;

  winrt::com_ptr<IWICImagingFactory> mWicf;
  winrt::com_ptr<ID2D1Factory> mD2df;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "OpenKneeboard Test Viewer",
      wxDefaultPosition,
      wxSize {768 / 2, 1024 / 2},
      wxDEFAULT_FRAME_STYLE) {
    Bind(wxEVT_PAINT, &MainWindow::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, [this](auto&) {});
    Bind(
      wxEVT_MENU, [this](auto&) { this->Close(true); }, wxID_EXIT);
    Bind(wxEVT_SIZE, [this](auto&) { this->Refresh(); });

    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      fileMenu->Append(wxID_EXIT, _T("E&xit"));
      menuBar->Append(fileMenu, _("&File"));
    }
    SetMenuBar(menuBar);

    mTimer.Bind(wxEVT_TIMER, [this](auto) { this->CheckForUpdate(); });
    mTimer.Start(100);
  }

  void CheckForUpdate() {
    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      if (mFirstDetached) {
        Refresh();
      }
      return;
    }

    if (snapshot.GetHeader()->SequenceNumber != mLastSequenceNumber) {
      Refresh();
      Update();
    }
  }

  void OnPaint(wxPaintEvent& ev) {
    const auto clientSize = GetClientSize();

    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      wxBufferedPaintDC dc(this);
      if (!mHadData) {
        dc.Clear();
      } else if (mFirstDetached) {
        mFirstDetached = false;
        auto bm = dc.GetAsBitmap().ConvertToDisabled();
        dc.DrawBitmap(bm, wxPoint {0, 0});
      }
      RenderError(this->GetClientSize(), dc, "No Feeder");
      return;
    }
    mHadData = true;
    mFirstDetached = true;

    const auto& config = *snapshot.GetHeader();
    const auto pixels = snapshot.GetPixels();

    wxPaintDC dc(this);

    if (!mWicf) {
      mWicf
        = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
    }
    winrt::com_ptr<IWICBitmap> wicBitmap;
    mWicf->CreateBitmapFromMemory(
      config.ImageWidth,
      config.ImageHeight,
      GUID_WICPixelFormat32bppRGBA,
      config.ImageWidth * sizeof(SHM::Pixel),
      config.ImageWidth * config.ImageHeight * sizeof(SHM::Pixel),
      reinterpret_cast<BYTE*>(const_cast<SHM::Pixel*>(pixels)),
      wicBitmap.put());

    if (!mD2df) {
      D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        mD2df.put_void());
    }

    auto rtp = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1_PIXEL_FORMAT {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED,
      });
    rtp.dpiX = 0;
    rtp.dpiY = 0;

    D2D1_SIZE_U size {
      static_cast<UINT32>(clientSize.GetWidth()),
      static_cast<UINT32>(clientSize.GetHeight())};

    auto hwndRtp = D2D1::HwndRenderTargetProperties(this->GetHWND(), size);

    winrt::com_ptr<ID2D1HwndRenderTarget> rt;
    mD2df->CreateHwndRenderTarget(&rtp, &hwndRtp, rt.put());

    winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
    rt->CreateBitmapFromWicBitmap(
      wicBitmap.get(),
      D2D1_BITMAP_PROPERTIES {
        {DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0},
      d2dBitmap.put());

    rt->BeginDraw();
    rt->SetTransform(D2D1::Matrix3x2F::Identity());

    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    rt->Clear(D2D1_COLOR_F {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    });

    const auto scalex = float(clientSize.GetWidth()) / config.ImageWidth;
    const auto scaley = float(clientSize.GetHeight()) / config.ImageHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = config.ImageWidth * scale;
    const auto renderHeight = config.ImageHeight * scale;

    const auto renderLeft = (clientSize.GetWidth() - renderWidth) / 2;
    const auto renderTop = (clientSize.GetHeight() - renderHeight) / 2;
    D2D1_RECT_F pageRect {
      renderLeft,
      renderTop,
      renderLeft + renderWidth,
      renderTop + renderHeight};

    rt->DrawBitmap(
      d2dBitmap.get(), &pageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    rt->EndDraw();

    mLastSequenceNumber = config.SequenceNumber;
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
