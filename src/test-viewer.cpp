#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/frame.h>

#include "YAVRK/SHM.h"

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;
  bool mFirstDetached = false;
  bool mHadData = false;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "YAVRK Test Viewer",
      wxDefaultPosition,
      wxSize {768 / 2, 1024 / 2},
      wxDEFAULT_FRAME_STYLE) {
    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      fileMenu->Append(wxID_EXIT, _T("E&xit"));
      menuBar->Append(fileMenu, _("&File"));
    }
    SetMenuBar(menuBar);

    mTimer.Bind(wxEVT_TIMER, [this](auto) { this->Refresh(); });
    mTimer.Start(100);
    this->Bind(wxEVT_ERASE_BACKGROUND, [this](auto) {});
  }

  void OnPaint(wxPaintEvent& ev) {
    wxBufferedPaintDC dc(this);
    const auto clientSize = GetClientSize();

    auto shm = YAVRK::SHM::MaybeGet();
    if (!shm) {
      if (!mHadData) {
        dc.Clear();
      } else if (mFirstDetached) {
        mFirstDetached = false;
        auto bm = dc.GetAsBitmap().ConvertToDisabled();
        dc.DrawBitmap(bm, wxPoint{0, 0});
      }
      std::string message("No Feeder");
      auto textSize = dc.GetTextExtent(message);
      auto textOrigin = wxPoint {
        (clientSize.GetWidth() - textSize.GetWidth()) / 2,
        (clientSize.GetHeight() - textSize.GetHeight()) / 2};

      dc.SetPen(wxPen(*wxBLACK, 2));
      auto boxSize
        = wxSize {textSize.GetWidth() + 20, textSize.GetHeight() + 20};
      auto boxOrigin = wxPoint {
        (clientSize.GetWidth() - boxSize.GetWidth()) / 2,
        (clientSize.GetHeight() - boxSize.GetHeight()) / 2};

      dc.SetBrush(*wxGREY_BRUSH);
      dc.DrawRectangle(boxOrigin, boxSize);

      dc.SetBrush(*wxBLACK_BRUSH);
      dc.DrawText(message, textOrigin);

      return;
    }
    mHadData = true;
    mFirstDetached = true;

    dc.Clear();
    auto config = shm.Header();
    wxImage image(config.ImageWidth, config.ImageHeight);
    image.SetAlpha(nullptr, true);

    using Pixel = YAVRK::SHM::Pixel;
    std::vector<Pixel> pixels(config.ImageWidth * config.ImageHeight);
    memcpy(reinterpret_cast<void*>(pixels.data()), shm.ImageData(), pixels.size() * sizeof(Pixel));
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
  }

  void OnExit(wxCommandEvent& ev) {
    Close(true);
  }

  wxDECLARE_EVENT_TABLE();
};

// clang-format off
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
  EVT_PAINT(MainWindow::OnPaint)
  EVT_MENU(wxID_EXIT, MainWindow::OnExit)
wxEND_EVENT_TABLE();
// clang-format off

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
