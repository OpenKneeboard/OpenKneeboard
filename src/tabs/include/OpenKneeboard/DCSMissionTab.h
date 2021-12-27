#pragma once

#include "DCSTab.h"

namespace OpenKneeboard {

class DCSMissionTab final : public DCSTab {
 public:
  DCSMissionTab();
  virtual ~DCSMissionTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual wxImage RenderPage(uint16_t index) override;

 protected:
  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path&,
    const std::filesystem::path&,
    const std::string&) override;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard
