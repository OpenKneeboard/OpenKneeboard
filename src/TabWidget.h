#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

#include <memory>

namespace YAVRK {
  class Tab;
}

class TabWidget final : public wxPanel {
  public:
    TabWidget(wxWindow* parent, const std::shared_ptr<YAVRK::Tab>&);
    virtual ~TabWidget();

    void OnPaint(wxPaintEvent& ev);
    void OnEraseBackground(wxEraseEvent& ev);

    uint16_t GetPageIndex() const;
    void SetPageIndex(uint16_t index);
    void NextPage();
    void PreviousPage();
    std::shared_ptr<YAVRK::Tab> GetTab() const;

    wxDECLARE_EVENT_TABLE();
  private:
    class Impl;
    std::shared_ptr<Impl> p;
};
