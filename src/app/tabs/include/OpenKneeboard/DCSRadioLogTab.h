#pragma once

#include <OpenKneeboard/DCSTab.h>

namespace OpenKneeboard {

class FolderTab;

class DCSRadioLogTab final : public DCSTab {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  DCSRadioLogTab();
  virtual ~DCSRadioLogTab();

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
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    const std::string& value) override;

  virtual void OnSimulationStart() override;
};

}// namespace OpenKneeboard
