#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

#include <memory>
#include <string>

namespace OpenKneeboard {
class Tab {
 public:
  Tab(const std::string& title);
  virtual ~Tab();

  std::string GetTitle() const;

  virtual void Reload();
  virtual uint16_t GetPageCount() const = 0;
  virtual wxImage RenderPage(uint16_t index) = 0;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
