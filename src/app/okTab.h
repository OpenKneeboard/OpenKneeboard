#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>

namespace OpenKneeboard {
class Tab;
}

class okTab final : public wxPanel {
 public:
  okTab(wxWindow* parent, const std::shared_ptr<OpenKneeboard::Tab>&);
  virtual ~okTab();

  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;
  wxImage GetImage();
 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void EmitPageChanged();
};
