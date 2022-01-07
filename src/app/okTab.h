#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>

#include <winrt/base.h>
#include <d2d1.h>

namespace OpenKneeboard {
class Tab;
}

class okTab final : public wxPanel {
 public:
  okTab(wxWindow* parent, const std::shared_ptr<OpenKneeboard::Tab>&);
  virtual ~okTab();

  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;

  void Render(
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect);

  void NextPage();
  void PreviousPage();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void EmitPageChanged();
};
