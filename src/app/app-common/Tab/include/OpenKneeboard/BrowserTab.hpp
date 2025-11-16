// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ChromiumPageSource.hpp>
#include <OpenKneeboard/ITabWithSettings.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>
#include <OpenKneeboard/TabBase.hpp>
#include <OpenKneeboard/WebPageSourceSettings.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

class HWNDPageSource;

class BrowserTab final : public TabBase,
                         public PageSourceWithDelegates,
                         public virtual ITabWithSettings {
 public:
  using Settings = WebPageSourceSettings;

  BrowserTab() = delete;
  static task<std::shared_ptr<BrowserTab>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    winrt::guid persistentID,
    std::string_view title,
    Settings);
  ~BrowserTab() override;

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  [[nodiscard]]
  virtual task<void> Reload() final override;

  nlohmann::json GetSettings() const override;

  bool IsSimHubIntegrationEnabled() const;
  [[nodiscard]]
  task<void> SetSimHubIntegrationEnabled(bool);

  bool AreOpenKneeboardAPIsEnabled() const;
  [[nodiscard]]
  task<void> SetOpenKneeboardAPIsEnabled(bool);

  bool IsBackgroundTransparent() const;
  [[nodiscard]]
  task<void> SetBackgroundTransparent(bool);

 private:
  BrowserTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const Settings&);
  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings;
  bool mHaveTitle {false};

  std::shared_ptr<ChromiumPageSource> mDelegate;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(BrowserTab::Settings);

}// namespace OpenKneeboard
