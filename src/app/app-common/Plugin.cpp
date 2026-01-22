// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/Plugin.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/json.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

namespace OpenKneeboard {

std::string Plugin::GetIDHash() const noexcept {
  using namespace winrt::Windows::Security::Cryptography::Core;
  using namespace winrt::Windows::Security::Cryptography;
  const auto size = static_cast<uint32_t>(mID.size());
  winrt::Windows::Storage::Streams::Buffer buffer {size};
  buffer.Length(size);
  memcpy(buffer.data(), mID.data(), mID.size());

  const auto hashProvider =
    HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256());
  auto hashObj = hashProvider.CreateHash();
  hashObj.Append(buffer);
  return winrt::to_string(
    winrt::Windows::Security::Cryptography::CryptographicBuffer::
      EncodeToHexString(hashObj.GetValueAndReset()));
}

OPENKNEEBOARD_DEFINE_JSON(
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
    const auto& args =
      std::get<InvalidImplementationArgs>(v.mImplementationArgs);
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
