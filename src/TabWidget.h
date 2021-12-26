#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>

namespace OpenKneeboard {
  class Tab;
}

wxDECLARE_EVENT(OPENKNEEBOARD_PAGE_CHANGED, wxCommandEvent);

class TabWidget final : public wxPanel {
  public:
    TabWidget(wxWindow* parent, const std::shared_ptr<OpenKneeboard::Tab>&);
    virtual ~TabWidget();

    std::shared_ptr<OpenKneeboard::Tab> GetTab() const;
    wxImage GetImage();

    wxDECLARE_EVENT_TABLE();
  private:
    class Impl;
    std::shared_ptr<Impl> p;

    void EmitPageChanged();
};
