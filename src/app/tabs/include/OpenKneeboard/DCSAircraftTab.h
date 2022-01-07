#pragma once

#include "OpenKneeboard/DCSTab.h"

namespace OpenKneeboard {

class FolderTab;

class DCSAircraftTab final : public DCSTab {
 public:
  DCSAircraftTab();
  virtual ~DCSAircraftTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual void RenderPage(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect) override;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) override;

 protected:
  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path&,
    const std::filesystem::path&,
    const std::string&) override;

 private:
  std::shared_ptr<FolderTab> mDelegate;
};

}// namespace OpenKneeboard
