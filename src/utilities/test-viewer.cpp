#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <D2d1.h>
#include <unknwn.h>
#include <winrt/base.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/graphics.h>

#include "OpenKneeboard/D2DErrorRenderer.h"
#include "OpenKneeboard/SHM.h"

#pragma comment(lib, "D2d1.lib")

using namespace OpenKneeboard;

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;
  bool mFirstDetached = false;
  SHM::Reader mSHM;
  uint64_t mLastSequenceNumber = 0;

  winrt::com_ptr<ID2D1Factory> mD2df;
  winrt::com_ptr<ID2D1HwndRenderTarget> mRt;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "OpenKneeboard Test Viewer",
      wxDefaultPosition,
      wxSize {768 / 2, 1024 / 2},
      wxDEFAULT_FRAME_STYLE) {
    D2D1CreateFactory(
      D2D1_FACTORY_TYPE_SINGLE_THREADED,
      __uuidof(ID2D1Factory),
      mD2df.put_void());
    mErrorRenderer = std::make_unique<D2DErrorRenderer>(mD2df);

    Bind(wxEVT_PAINT, &MainWindow::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, [this](auto&) {});
    Bind(
      wxEVT_MENU, [this](auto&) { this->Close(true); }, wxID_EXIT);
    Bind(wxEVT_SIZE, &MainWindow::OnSize, this);

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
        Update();
      }
      return;
    }

    if (snapshot.GetHeader()->SequenceNumber != mLastSequenceNumber) {
      Refresh();
      Update();
    }
  }

  void OnSize(wxSizeEvent& ev) {
    if (!this->mRt) {
      return;
    }
    auto cs = this->GetClientSize();
    this->mRt->Resize(
      {static_cast<UINT>(cs.GetWidth()), static_cast<UINT>(cs.GetHeight())});
    this->Refresh();
  }

  void OnPaint(wxPaintEvent& ev) {
    const auto clientSize = GetClientSize();

    if (!mRt) {
      auto rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1_PIXEL_FORMAT {
          DXGI_FORMAT_B8G8R8A8_UNORM,
          D2D1_ALPHA_MODE_IGNORE,
        });

      auto hwndRtp = D2D1::HwndRenderTargetProperties(
        this->GetHWND(),
        {static_cast<UINT32>(clientSize.GetWidth()),
         static_cast<UINT32>(clientSize.GetHeight())});

      mD2df->CreateHwndRenderTarget(&rtp, &hwndRtp, mRt.put());
      mErrorRenderer->SetRenderTarget(mRt);

      winrt::com_ptr<ID2D1Bitmap> backgroundBitmap;
      SHM::Pixel pixels[20 * 20];
      for (int x = 0; x < 20; x++) {
        for (int y = 0; y < 20; y++) {
          bool white = (x < 10 && y < 10) || (x >= 10 && y >= 10);
          uint8_t value = white ? 0xff : 0xcc;
          pixels[x + (20 * y)] = {value, value, value, 0xff};
        }
      }
      mRt->CreateBitmap(
        {20, 20},
        reinterpret_cast<BYTE*>(pixels),
        20 * sizeof(SHM::Pixel),
        D2D1::BitmapProperties(D2D1::PixelFormat(
          DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        backgroundBitmap.put());

      mBackgroundBrush = nullptr;
      mRt->CreateBitmapBrush(
        backgroundBitmap.get(),
        D2D1::BitmapBrushProperties(
          D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP),
        reinterpret_cast<ID2D1BitmapBrush**>(mBackgroundBrush.put()));
    }

    wxPaintDC dc(this);
    mRt->BeginDraw();
    wxON_BLOCK_EXIT0([this]() { this->mRt->EndDraw(); });
    mRt->SetTransform(D2D1::Matrix3x2F::Identity());

    auto wxBgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    D2D1_COLOR_F bgColor {
      wxBgColor.Red() / 255.0f,
      wxBgColor.Green() / 255.0f,
      wxBgColor.Blue() / 255.0f,
      1.0f};
    mRt->Clear(bgColor);

    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      mErrorRenderer->Render(
        _("No Feeder").ToStdWstring(),
        {0.0f,
         0.0f,
         float(clientSize.GetWidth()),
         float(clientSize.GetHeight())});
      mFirstDetached = true;
      return;
    }
    mFirstDetached = false;

    const auto& config = *snapshot.GetHeader();
    const auto pixels = snapshot.GetPixels();

    if (config.ImageWidth == 0 || config.ImageHeight == 0) {
      mErrorRenderer->Render(
        _("Invalid Image").ToStdWstring(),
        {0.0f,
         0.0f,
         float(clientSize.GetWidth()),
         float(clientSize.GetHeight())});
    }

    winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
    mRt->CreateBitmap(
      {config.ImageWidth, config.ImageHeight},
      reinterpret_cast<const void*>(pixels),
      config.ImageWidth * sizeof(SHM::Pixel),
      D2D1_BITMAP_PROPERTIES {
        {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0},
      d2dBitmap.put());

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

    auto bg = mBackgroundBrush;
    // Align the top-left pixel of the brush
    bg->SetTransform(
      D2D1::Matrix3x2F::Translation({pageRect.left, pageRect.top}));
    mRt->FillRectangle(pageRect, bg.get());
    mRt->DrawBitmap(
      d2dBitmap.get(), &pageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

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
