#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/frame.h>

#include "YAVRK/SHM.h"

struct R8G8B8A8 {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

class MainWindow final : public wxFrame {
 private:
  wxTimer mTimer;

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
    dc.clear();

    auto shm = YAVRK::SHM::MaybeGet();
    if (!shm) {
      // TODO: render error
      return;
    }

    dc.Clear();
    auto config = shm.Header();
    wxImage image(config.ImageWidth, config.ImageHeight);
    image.SetAlpha(nullptr, true);
    auto pixels = reinterpret_cast<R8G8B8A8*>(shm.ImageData());
    for (uint16_t y = 0; y < config.ImageHeight; ++y) {
      for (uint16_t x = 0; x < config.ImageWidth; ++x) {
        auto pixel = pixels[x + (y * config.ImageWidth)];
        image.SetRGB(x, y, pixel.r, pixel.g, pixel.b);
        image.SetAlpha(x, y, pixel.a);
      }
    }

    const auto renderSize = GetClientSize();
    const auto scalex = float(renderSize.GetWidth()) / config.ImageWidth;
    const auto scaley = float(renderSize.GetHeight()) / config.ImageHeight;
    const auto scale = std::min(scalex, scaley);
    auto scaled = image.Scale(
      int(config.ImageWidth * scale),
      int(config.ImageHeight * scale),
      wxIMAGE_QUALITY_HIGH);

    wxPoint origin {
      (renderSize.GetWidth() - scaled.GetWidth()) / 2,
      (renderSize.GetHeight() - scaled.GetHeight()) / 2,
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
