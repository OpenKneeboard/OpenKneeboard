#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>

namespace OpenKneeboard {
class Tab;
}

wxDECLARE_EVENT(okEVT_PAGE_CHANGED, wxCommandEvent);

class okTab final : public wxPanel {
 public:
  okTab(wxWindow* parent, const std::shared_ptr<OpenKneeboard::Tab>&);
  virtual ~okTab();

  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;
  wxImage GetImage();

  void NextPage();
  void PreviousPage();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void EmitPageChanged();
};
