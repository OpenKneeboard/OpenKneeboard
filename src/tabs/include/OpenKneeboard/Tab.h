#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

#include <memory>
#include <string>

wxDECLARE_EVENT(okEVT_TAB_UPDATED, wxCommandEvent);

namespace OpenKneeboard {
struct GameEvent;

class Tab : public wxEvtHandler {
 public:
  Tab(const wxString& title);
  virtual ~Tab();

  std::string GetTitle() const;

  virtual void Reload();
  virtual void OnGameEvent(const GameEvent&);
  virtual uint16_t GetPageCount() const = 0;
  virtual wxImage RenderPage(uint16_t index) = 0;
 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
