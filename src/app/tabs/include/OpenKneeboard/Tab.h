#pragma once

#include "okConfigurableComponent.h"

#include <memory>
#include <string>

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
  virtual wxImage RenderPage(uint16_t index) = 0;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
