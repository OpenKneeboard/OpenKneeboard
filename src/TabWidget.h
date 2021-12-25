#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>

namespace YAVRK {
  class Tab;
}

class TabWidget final : public wxPanel {
  public:
    TabWidget(wxWindow* parent, const std::shared_ptr<YAVRK::Tab>&);
    virtual ~TabWidget();

    std::shared_ptr<YAVRK::Tab> GetTab() const;
    wxImage GetImage();

    wxDECLARE_EVENT_TABLE();
  private:
    class Impl;
    std::shared_ptr<Impl> p;
};
