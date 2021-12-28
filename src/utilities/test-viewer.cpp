#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/frame.h>

#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/RenderError.h"

using namespace OpenKneeboard;

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;
  bool mFirstDetached = false;
  bool mHadData = false;
  SHM::Reader mSHM;
  uint64_t mLastSequenceNumber = 0;

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
    Bind(wxEVT_ERASE_BACKGROUND, [this](auto) {});
    Bind(
      wxEVT_MENU, [this](auto) { this->Close(true); }, wxID_EXIT);

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
    }
  }

  void OnPaint(wxPaintEvent& ev) {
    wxBufferedPaintDC dc(this);
    const auto clientSize = GetClientSize();

    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
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

    dc.Clear();
    wxImage image(config.ImageWidth, config.ImageHeight);
    image.SetAlpha(nullptr, true);

    for (uint16_t y = 0; y < config.ImageHeight; ++y) {
      for (uint16_t x = 0; x < config.ImageWidth; ++x) {
        auto pixel = pixels[x + (y * config.ImageWidth)];
        image.SetRGB(x, y, pixel.r, pixel.g, pixel.b);
        image.SetAlpha(x, y, pixel.a);
      }
    }

    const auto scalex = float(clientSize.GetWidth()) / config.ImageWidth;
    const auto scaley = float(clientSize.GetHeight()) / config.ImageHeight;
    const auto scale = std::min(scalex, scaley);
    auto scaled = image.Scale(
      int(config.ImageWidth * scale),
      int(config.ImageHeight * scale),
      wxIMAGE_QUALITY_HIGH);

    wxPoint origin {
      (clientSize.GetWidth() - scaled.GetWidth()) / 2,
      (clientSize.GetHeight() - scaled.GetHeight()) / 2,
    };

    dc.DrawBitmap(scaled, origin);
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
