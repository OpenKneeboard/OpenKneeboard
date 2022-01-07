#pragma once

#include "OpenKneeboard/DCSTab.h"

namespace OpenKneeboard {

class FolderTab;

class DCSRadioLogTab final : public DCSTab {
 private:
  std::vector<std::vector<std::string>> mCompletePages;
  std::vector<std::string> mCurrentPageLines;
  std::vector<std::string> mMessages;
  int mColumns = -1;
  int mRows = -1;

  void PushMessage(const std::string& message);
  void LayoutMessages();
  void PushPage();
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
