// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/UISettings.hpp>

#include <OpenKneeboard/json_fwd.hpp>

#include <shims/optional>

#include <Windows.h>

#include <filesystem>

namespace OpenKneeboard {

struct AutoUpdateSettings final {
  static constexpr std::string_view StableChannel {"stable"};
  static constexpr std::string_view PreviewChannel {"preview"};

  uint64_t mDisabledUntil {0};
  std::string mSkipVersion {};
  std::string mChannel {StableChannel};

  struct Testing final {
    std::string mBaseURI {};
    std::string mFakeCurrentVersion {};
    std::string mFakeUpdateVersion {};
    bool mAlwaysCheck {false};

    constexpr bool operator==(const Testing&) const noexcept = default;
  };

  Testing mTesting {};

  constexpr bool operator==(const AutoUpdateSettings&) const noexcept = default;
};

struct AppSettings final {
  std::optional<RECT> mWindowRect;
  AutoUpdateSettings mAutoUpdate {};
  std::string mLastRunVersion;
  bool mAlwaysShowDeveloperTools {false};

  struct Deprecated {
    struct DualKneeboardSettings final {
      bool mEnabled = false;
      constexpr bool operator==(const DualKneeboardSettings&) const noexcept
        = default;
    };
    DualKneeboardSettings mDualKneeboards {};

    constexpr bool operator==(const Deprecated&) const noexcept = default;
  };
  Deprecated mDeprecated;

  bool operator==(const AppSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(AppSettings);

}// namespace OpenKneeboard
