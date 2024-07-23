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
#include <OpenKneeboard/Plugin.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Plugin::Metadata,
  mPluginName,
  mPluginReadableVersion,
  mPluginSemanticVersion,
  mOKBMinimumVersion,
  mOKBMaximumTestedVersion,
  mAuthor,
  mWebsite)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(Plugin::CustomAction, mID, mName)

NLOHMANN_JSON_SERIALIZE_ENUM(
  Plugin::TabType::Implementation,
  {{Plugin::TabType::Implementation::Invalid, "Invalid"},
   {
     Plugin::TabType::Implementation::WebBrowser,
     "WebBrowser",
   }})

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Plugin::TabType::WebBrowserArgs,
  mURI,
  mInitialSize);

template <>
void from_json_postprocess<Plugin::TabType>(
  const nlohmann::json& j,
  Plugin::TabType& v) {
  using Implementation = Plugin::TabType::Implementation;
  const auto args = j.at("ImplementationArgs");
  switch (v.mImplementation) {
    case Implementation::Invalid:
      v.mImplementationArgs = Plugin::TabType::InvalidImplementationArgs {
        .mName = j.at("Implementation"),
        .mArgs = args,
      };
      break;
    case Implementation::WebBrowser:
      v.mImplementationArgs = args.get<Plugin::TabType::WebBrowserArgs>();
      break;
  }
}

void to_json(nlohmann::json& j, const Plugin::TabType::ImplementationArgs& v) {
  if (std::holds_alternative<Plugin::TabType::EmptyArgs>(v)) {
    return;
  }

  using InvalidImplementationArgs = Plugin::TabType::InvalidImplementationArgs;
  if (std::holds_alternative<InvalidImplementationArgs>(v)) {
    j.update(std::get<InvalidImplementationArgs>(v));
    return;
  }

  using WebBrowserArgs = Plugin::TabType::WebBrowserArgs;
  if (std::holds_alternative<WebBrowserArgs>(v)) {
    to_json(j, std::get<WebBrowserArgs>(v));
    return;
  }

  OPENKNEEBOARD_BREAK;
}

template <>
void to_json_postprocess<Plugin::TabType>(
  nlohmann::json& j,
  const Plugin::TabType& v) {
  using Implementation = Plugin::TabType::Implementation;

  to_json(j["ImplementationArgs"], v.mImplementationArgs);

  using InvalidImplementationArgs = Plugin::TabType::InvalidImplementationArgs;
  if (
    v.mImplementation == Implementation::Invalid
    && std::holds_alternative<InvalidImplementationArgs>(
      v.mImplementationArgs)) {
    const auto& args
      = std::get<InvalidImplementationArgs>(v.mImplementationArgs);
    j["Implementation"] = args.mName;
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Plugin::TabType,
  mID,
  mName,
  mGlyph,
  mCategoryID,
  mCustomActions,
  mImplementation)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(Plugin, mID, mMetadata, mTabTypes)

}// namespace OpenKneeboard