/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/json_fwd.hpp>

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

      bool operator==(const InvalidImplementationArgs&) const noexcept
        = default;
    };
    struct WebBrowserArgs {
      std::string mURI;
      PixelSize mInitialSize {1024, 768};

      bool operator==(const WebBrowserArgs&) const noexcept = default;
    };
    using ImplementationArgs
      = std::variant<EmptyArgs, InvalidImplementationArgs, WebBrowserArgs>;

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

  bool operator==(const Plugin&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(Plugin);

}// namespace OpenKneeboard