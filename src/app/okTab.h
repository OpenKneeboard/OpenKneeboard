#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <d2d1.h>
#include <winrt/base.h>

#include <memory>

namespace OpenKneeboard {
class Tab;
}

class okTab final : public wxPanel {
 public:
  okTab(wxWindow* parent, const std::shared_ptr<OpenKneeboard::Tab>&);
  virtual ~okTab();

  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;
  uint16_t GetPageIndex() const;

  void NextPage();
  void PreviousPage();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void EmitPageChanged();
};
