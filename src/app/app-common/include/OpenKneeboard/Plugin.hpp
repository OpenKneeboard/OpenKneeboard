// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/json/Geometry2D.hpp>

#include <OpenKneeboard/json.hpp>

#include <string>
#include <variant>
#include <vector>

namespace OpenKneeboard {

struct Plugin {
  struct Metadata {
    std::string mPluginName;
    std::string mPluginReadableVersion;
    std::string mPluginSemanticVersion;
    std::string mOKBMinimumVersion {"1.9"};
    std::string mOKBMaximumTestedVersion;

    std::string mAuthor;
    std::string mWebsite;

    bool operator==(const Metadata&) const noexcept = default;
  };

  struct CustomAction {
    std::string mID;
    std::string mName;

    bool operator==(const CustomAction&) const noexcept = default;
  };

  struct TabType {
    enum class Implementation {
      Invalid,
      WebBrowser,
    };

    using EmptyArgs = std::monostate;
    struct InvalidImplementationArgs {
      std::string mName;
      nlohmann::json mArgs;

      bool operator==(const InvalidImplementationArgs&) const noexcept =
        default;
    };
    struct WebBrowserArgs {
      std::string mURI;
      PixelSize mInitialSize {1024, 768};

      bool operator==(const WebBrowserArgs&) const noexcept = default;
    };
    using ImplementationArgs =
      std::variant<EmptyArgs, InvalidImplementationArgs, WebBrowserArgs>;

    std::string mID;
    std::string mName;
    std::string mGlyph;
    std::string mCategoryID;

    std::vector<CustomAction> mCustomActions;

    Implementation mImplementation {Implementation::Invalid};
    ImplementationArgs mImplementationArgs {};

    bool operator==(const TabType&) const noexcept = default;
  };

  std::string mID;
  Metadata mMetadata;
  std::vector<TabType> mTabTypes;

  std::string GetIDHash() const noexcept;
  std::filesystem::path mJSONPath;

  bool operator==(const Plugin&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(Plugin);

}// namespace OpenKneeboard
