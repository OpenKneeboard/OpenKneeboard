#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "okMainWindow.h"

class OpenKneeboardApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    wxInitAllImageHandlers();
    (new okMainWindow())->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OpenKneeboardApp);
