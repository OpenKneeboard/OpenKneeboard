#pragma once

#include <d2d1.h>
#include <winrt/base.h>

#include <memory>
#include <string>

#include "okConfigurableComponent.h"

namespace OpenKneeboard {
struct GameEvent;

class Tab : public okConfigurableComponent {
 public:
  Tab(const wxString& title);
  virtual ~Tab();

  std::string GetTitle() const;

  virtual void Reload();
  virtual void OnGameEvent(const GameEvent&);

  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;

  virtual uint16_t GetPageCount() const = 0;
  virtual void RenderPage(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect)
    = 0;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) = 0;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
