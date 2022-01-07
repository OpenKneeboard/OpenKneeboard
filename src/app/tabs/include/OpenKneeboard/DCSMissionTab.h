#pragma once

#include "DCSTab.h"

namespace OpenKneeboard {

class DCSMissionTab final : public DCSTab {
 public:
  DCSMissionTab();
  virtual ~DCSMissionTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual void RenderPage(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect) final override;

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
