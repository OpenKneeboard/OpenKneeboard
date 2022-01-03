#pragma once

#include "OpenKneeboard/DCSTab.h"

namespace OpenKneeboard {

class FolderTab;

class DCSRadioLogTab final : public DCSTab {
 private:
  std::vector<std::string> mMessages;
  int mColumns = -1;

 public:
  DCSRadioLogTab();
  virtual ~DCSRadioLogTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual wxImage RenderPage(uint16_t index) override;

 protected:
  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    const std::string& value) override;

  virtual void OnSimulationStart() override;
};

}// namespace OpenKneeboard
