// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

// clang-format off
#include "pch.h"
#include "InstallPlugin.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/Plugin.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/PluginTab.hpp>
#include <OpenKneeboard/TabsList.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/semver.hpp>
#include <OpenKneeboard/utf8.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <shellapi.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <wil/resource.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <unordered_set>

#include <processenv.h>
#include <zip.h>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace {

enum class PluginInstallAction {
  Install,
  Update,
  NothingToDo,// Already up to date.
};

using unique_zip_ptr
  = wil::unique_any<zip_t*, decltype(&zip_close), &zip_close>;
using unique_zip_error
  = wil::unique_struct<zip_error_t, decltype(&zip_error_fini), &zip_error_fini>;
using unique_zip_file
  = wil::unique_any<zip_file_t*, decltype(&zip_fclose), &zip_fclose>;
}// namespace

namespace OpenKneeboard {
static task<void> ShowPluginInstallationError(
  XamlRoot xamlRoot,
  std::filesystem::path path,
  std::string_view error) {
  dprint.Error("Plugin installation error for `{}`: {}", path, error);

  StackPanel layout;
  layout.Margin({8, 8, 8, 8});
  layout.Spacing(12);
  {
    TextBlock p;
    p.Text(std::format(_(L"Couldn't install `{}`:"), path.filename()));
    p.TextWrapping(TextWrapping::WrapWholeWords);
    layout.Children().Append(p);
  }
  {
    TextBlock p;
    p.Text(winrt::to_hstring(error));
    p.TextWrapping(TextWrapping::WrapWholeWords);
    layout.Children().Append(p);
  }

  ContentDialog dialog;
  dialog.XamlRoot(xamlRoot);
  dialog.Title(winrt::box_value(to_hstring(_(L"Install Plugin"))));
  dialog.Content(layout);
  dialog.CloseButtonText(_(L"OK"));
  dialog.DefaultButton(ContentDialogButton::Close);

  co_await dialog.ShowAsync();

  co_return;
}

template <class... Args>
  requires(sizeof...(Args) >= 1)
static task<void> ShowPluginInstallationError(
  XamlRoot xamlRoot,
  std::filesystem::path path,
  std::format_string<Args...> errorFmt,
  Args&&... args) {
  return ShowPluginInstallationError(
    xamlRoot, path, std::format(errorFmt, std::forward<Args>(args)...));
}

static task<bool> FieldIsNonEmpty(
  XamlRoot xamlRoot,
  const auto path,
  const auto fieldName,
  auto value) {
  if (value == decltype(value) {}) {
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Field `{}` is required, and must not non-empty."), fieldName));
    co_return false;
  }
  co_return true;
}

static PluginInstallAction GetPluginInstallAction(
  KneeboardState* kneeboard,
  const Plugin& plugin) {
  const auto installedPlugins = kneeboard->GetPluginStore()->GetPlugins();
  const auto it = std::ranges::find(installedPlugins, plugin.mID, &Plugin::mID);
  if (it == installedPlugins.end()) {
    return PluginInstallAction::Install;
  }

  if (
    it->mMetadata.mPluginSemanticVersion
    == plugin.mMetadata.mPluginSemanticVersion) {
    return PluginInstallAction::NothingToDo;
  }

  return PluginInstallAction::Update;
}

static task<void> InstallPlugin(
  std::weak_ptr<KneeboardState> weakKneeboard,
  XamlRoot xamlRoot,
  std::filesystem::path path,
  Plugin plugin,
  zip_t* zip) {
  if (!co_await FieldIsNonEmpty(xamlRoot, path, "ID", plugin.mID)) {
    co_return;
  }

#define CHECK(FIELD) \
  if (!co_await FieldIsNonEmpty( \
        xamlRoot, path, #FIELD, plugin.mMetadata.m##FIELD)) { \
    co_return; \
  }
  CHECK(PluginName)
  CHECK(PluginReadableVersion)
  CHECK(PluginSemanticVersion)
  CHECK(OKBMinimumVersion)
#undef CHECK
  if (plugin.mTabTypes.empty()) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, "It contains no tab types, so does nothing");
    co_return;
  }

  const auto pluginIdPrefix = plugin.mID + ";";
  for (const auto& tab: plugin.mTabTypes) {
    if (!tab.mID.starts_with(pluginIdPrefix)) {
      co_await ShowPluginInstallationError(
        xamlRoot,
        path,
        "TabType ID `{}` must start with `{}` and doesn't",
        tab.mID,
        pluginIdPrefix);
      co_return;
    }
    const auto tabIdPrefix = tab.mID + ";";
    for (const auto& action: tab.mCustomActions) {
      if (!action.mID.starts_with(tabIdPrefix)) {
        co_await ShowPluginInstallationError(
          xamlRoot,
          path,
          "Custom action ID `{}` must start with `{}` and doesn't",
          action.mID,
          tabIdPrefix);
        co_return;
      }
    }
  }

  if (
    CompareVersions(plugin.mMetadata.mOKBMinimumVersion, Version::ReleaseName)
    == ThreeWayCompareResult::GreaterThan) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, "This plugin requires a newer version of OpenKneeboard.");
    co_return;
  }

  auto kneeboard = weakKneeboard.lock();
  if (!kneeboard) {
    dprint.Error("plugin store has gone away");
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  const auto action = GetPluginInstallAction(kneeboard.get(), plugin);
  switch (action) {
    case PluginInstallAction::NothingToDo: {
      ContentDialog dialog;
      dialog.XamlRoot(xamlRoot);
      dialog.Title(
        winrt::box_value(to_hstring(_(L"Plugin Already Installed"))));
      dialog.Content(
        winrt::box_value(to_hstring(
          std::format(
            _("Plugin '{}' v{} is already installed."),
            plugin.mMetadata.mPluginName,
            plugin.mMetadata.mPluginReadableVersion))));
      dialog.PrimaryButtonText(_(L"Tab Settings"));
      dialog.CloseButtonText(_(L"OK"));
      dialog.DefaultButton(ContentDialogButton::Close);

      if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
        co_await LaunchURI(
          std::format(
            "{}:///{}", SpecialURIs::Scheme, SpecialURIs::Paths::SettingsTabs));
      }
      co_return;
    }
    case PluginInstallAction::Install: {
      ContentDialog dialog;
      dialog.XamlRoot(xamlRoot);
      dialog.Title(winrt::box_value(to_hstring(_(L"Install Plugin?"))));
      dialog.Content(
        winrt::box_value(to_hstring(
          std::format(
            _("Do you want to install the plugin '{}'?"),
            plugin.mMetadata.mPluginName))));
      dialog.PrimaryButtonText(_(L"Install"));
      dialog.CloseButtonText(_(L"Cancel"));
      dialog.DefaultButton(ContentDialogButton::Close);

      if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
        co_return;
      }
      break;
    }

    case PluginInstallAction::Update: {
      ContentDialog dialog;
      dialog.XamlRoot(xamlRoot);
      dialog.Title(winrt::box_value(to_hstring(_(L"Update Plugin?"))));
      dialog.Content(
        winrt::box_value(to_hstring(
          std::format(
            _("Do you want to update the plugin '{}' to version {}? Any tabs "
              "from "
              "this plugin will be reloaded."),
            plugin.mMetadata.mPluginName,
            plugin.mMetadata.mPluginReadableVersion))));
      dialog.PrimaryButtonText(_(L"Update"));
      dialog.CloseButtonText(_(L"Cancel"));
      dialog.DefaultButton(ContentDialogButton::Primary);

      if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
        co_return;
      }
      break;
    }
  }

  const auto extractRoot
    = Filesystem::GetInstalledPluginsDirectory() / plugin.GetIDHash();
  if (std::filesystem::exists(extractRoot)) {
    dprint.Warning(
      "Removing previous plugin installation from {}", extractRoot);
    std::filesystem::remove_all(extractRoot);
  }
  dprint("Extracting `{}` to {}`", path, extractRoot);
  std::filesystem::create_directories(extractRoot);

  {
    const auto entryCount = zip_get_num_entries(zip, 0);
    zip_stat_t stat {};
    for (zip_int64_t i = 0; i < entryCount; ++i) {
      zip_stat_init(&stat);
      zip_stat_index(zip, i, 0, &stat);
      constexpr auto requiredValid = ZIP_STAT_NAME | ZIP_STAT_SIZE;
      if ((stat.valid & requiredValid) != requiredValid) {
        dprint.Error("Entry {} in zip does not have required metadata", i);
        OPENKNEEBOARD_BREAK;
        co_return;
      }

      unique_zip_file zipFile(zip_fopen_index(zip, i, 0));
      if (!zipFile) {
        continue;
      }
      const auto outPath = extractRoot / stat.name;
      if (outPath.filename().string().back() == '/') {
        std::filesystem::create_directories(outPath);
        continue;
      }
      std::filesystem::create_directories(outPath.parent_path());

      std::ofstream f(outPath, std::ios::binary);

      constexpr std::size_t chunkSize = 4 * 1024;// 4KB
      char buffer[chunkSize];

      decltype(stat.size) bytesRead = 0;
      while (bytesRead < stat.size) {
        const auto bytesReadThisChunk
          = zip_fread(zipFile.get(), buffer, std::size(buffer));
        if (bytesReadThisChunk <= 0) {
          const auto zipError = zip_file_get_error(zipFile.get());
          dprint.Error(
            "Failed to read chunk from file in zip: {} ({})",
            zip_error_strerror(zipError),
            zip_error_code_zip(zipError));
          OPENKNEEBOARD_BREAK;
          co_return;
        }
        f << std::string_view(buffer, bytesReadThisChunk);
        bytesRead += bytesReadThisChunk;
      }
    }
  }

  if (action == PluginInstallAction::Update) {
    const auto pluginTabIDs
      = std::views::transform(plugin.mTabTypes, &Plugin::TabType::mID)
      | std::ranges::to<std::unordered_set>();
    auto reloads = kneeboard->GetTabsList()->GetTabs()
      | std::views::transform([](auto tab) {
                     return std::dynamic_pointer_cast<PluginTab>(tab);
                   })
      | std::views::filter([pluginTabIDs](auto tab) {
                     if (!tab) {
                       return false;
                     }
                     return pluginTabIDs.contains(tab->GetPluginTabTypeID());
                   })
      | std::views::transform(&ITab::Reload) | std::ranges::to<std::vector>();
    for (auto&& reload: reloads) {
      co_await std::move(reload);
    }

    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(winrt::box_value(to_hstring(_(L"Plugin Updated"))));
    dialog.Content(
      winrt::box_value(to_hstring(
        std::format(
          _("'{}' has been updated to v{}"),
          plugin.mMetadata.mPluginName,
          plugin.mMetadata.mPluginReadableVersion))));
    dialog.PrimaryButtonText(_(L"Tab Settings"));
    dialog.CloseButtonText(_(L"OK"));
    dialog.DefaultButton(ContentDialogButton::Close);

    if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
      co_await LaunchURI(
        std::format(
          "{}:///{}", SpecialURIs::Scheme, SpecialURIs::Paths::SettingsTabs));
    }
    co_return;
  }
  OPENKNEEBOARD_ASSERT(action == PluginInstallAction::Install);

  plugin.mJSONPath = extractRoot / "v1.json";
  auto store = kneeboard->GetPluginStore();
  if (!store) {
    dprint.Error("plugin store has gone away");
    OPENKNEEBOARD_BREAK;
    co_return;
  }
  store->Append(plugin);

  {
    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(winrt::box_value(to_hstring(_(L"Plugin Installed"))));

    StackPanel layout;
    dialog.Content(layout);

    layout.Spacing(8);
    layout.Orientation(Orientation::Vertical);

    TextBlock caption;
    layout.Children().Append(caption);
    caption.Text(to_hstring(
      std::format(
        _("The plugin '{}' is now installed; would you like to add tabs from "
          "this plugin?"),
        plugin.mMetadata.mPluginName)));
    caption.TextWrapping(TextWrapping::WrapWholeWords);

    auto tabsToAppend = plugin.mTabTypes
      | std::views::transform(&Plugin::TabType::mID)
      | std::ranges::to<std::vector>();

    for (const auto& tabType: plugin.mTabTypes) {
      CheckBox checkBox;
      layout.Children().Append(checkBox);

      checkBox.IsChecked(true);
      checkBox.Content(
        box_value(to_hstring(std::format(_("Add a '{}' tab"), tabType.mName))));
      checkBox.Checked([&tabsToAppend, id = tabType.mID, dialog](auto&&...) {
        tabsToAppend.push_back(id);
        dialog.IsPrimaryButtonEnabled(!tabsToAppend.empty());
      });
      checkBox.Unchecked([&tabsToAppend, id = tabType.mID, dialog](auto&&...) {
        auto it = std::ranges::find(tabsToAppend, id);
        if (it != tabsToAppend.end()) {
          tabsToAppend.erase(it);
          dialog.IsPrimaryButtonEnabled(!tabsToAppend.empty());
        }
      });
    }

    dialog.PrimaryButtonText(_(L"Add tabs"));
    dialog.CloseButtonText(_(L"Close"));
    dialog.DefaultButton(ContentDialogButton::Primary);

    const auto result = co_await dialog.ShowAsync();
    if (result != ContentDialogResult::Primary) {
      co_return;
    }

    auto tabsStore = kneeboard->GetTabsList();
    auto tabs = tabsStore->GetTabs();
    for (const auto& id: tabsToAppend) {
      tabs.push_back(
        co_await PluginTab::Create(
          kneeboard->GetDXResources(),
          kneeboard.get(),
          {},
          std::ranges::find(plugin.mTabTypes, id, &Plugin::TabType::mID)->mName,
          {.mPluginTabTypeID = id}));
    }
    co_await tabsStore->SetTabs(tabs);
  }
}

static task<void> InstallPluginFromPath(
  std::weak_ptr<KneeboardState> kneeboard,
  XamlRoot xamlRoot,
  std::filesystem::path path) {
  dprint("Attempting to install plugin `{}`", path);
  if (!std::filesystem::exists(path)) {
    dprint.Error("asked to install plugin `{}`, which does not exist", path);
    co_return;
  }
  if (!std::filesystem::is_regular_file(path)) {
    dprint.Error(
      "asked to install plugin `{}`, which is not a regular file", path);
    co_return;
  }

  if (IsElevated() || IsShellElevated()) {
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      _("Plugins can not be installed while OpenKneeboard is running as "
        "administrator."));
    co_return;
  }

  int zipErrorCode {};
  unique_zip_ptr zip {
    zip_open(path.string().c_str(), ZIP_RDONLY, &zipErrorCode)};
  if (zipErrorCode) {
    unique_zip_error zerror;
    zip_error_init_with_code(&zerror, zipErrorCode);
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      _("Failed to open the file: \"{}\" ({}) "),
      zip_error_strerror(&zerror),
      zipErrorCode);
    co_return;
  }

  auto metadataFileIndex = zip_name_locate(zip.get(), "v1.json", 0);
  if (metadataFileIndex == -1) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, _("Plugin does not contain required metadata `v1.json`"));
    co_return;
  }

  zip_stat_t zstat;
  zip_stat_init(&zstat);
  zip_stat_index(zip.get(), metadataFileIndex, 0, &zstat);

  if ((zstat.valid & ZIP_STAT_SIZE) == 0) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, _("Metadata file `v1.json` has an unknown size"));
    co_return;
  }
  if (zstat.size > (1024 * 1024)) {
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      _("Metadata file `v1.json` has an uncompressed size of {} bytes, which "
        "is larger than the maximum of 1MB"),
      zstat.size);
    co_return;
  }

  unique_zip_file metadataFile {
    zip_fopen_index(zip.get(), metadataFileIndex, 0)};

  if (!metadataFile) {
    const auto error = zip_get_error(zip.get());
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Failed to open metadata file within plugin: \"{}\" ({})"),
        zip_error_strerror(error),
        zip_error_code_zip(error)));
    co_return;
  }

  std::string buf;
  buf.resize(zstat.size, '\0');
  // Okie, this is nasty, but need C pointers...
  const auto bufBegin = buf.data();
  const auto bufEnd = bufBegin + buf.size();

  for (auto it = buf.data(); it < bufEnd;) {
    const auto bytesRead = zip_fread(metadataFile.get(), it, bufEnd - it);
    if (bytesRead == 0) {
      if (it != bufEnd) {
        co_await ShowPluginInstallationError(
          xamlRoot,
          path,
          std::format(
            _("Read {} bytes from plugin metadata file, expected {} bytes"),
            buf.size(),
            zstat.size));
        co_return;
      }
      break;
    }
    if (bytesRead > 0) {
      it += bytesRead;
      continue;
    }
    // bytesRead < 0, error
    const auto error = zip_get_error(zip.get());
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Reading metadata file within plugin failed after {} bytes: \"{}\" "
          "({})"),
        bytesRead,
        zip_error_strerror(error),
        zip_error_code_zip(error)));
    co_return;
  }

  // We can't `co_await` to show a dialog in a catch block, so keep it until
  // later
  std::expected<Plugin, std::string> parseResult;
  try {
    const auto j = nlohmann::json::parse(buf);
    parseResult = j.get<Plugin>();

    const nlohmann::json roundTripJSON = *parseResult;
    const auto roundTripPlugin = roundTripJSON.get<Plugin>();
    if (roundTripPlugin != *parseResult) {
      dprint(
        "Plugin JSON round-trip mismatch\nOriginal JSON: {}\nRound-trip "
        "JSON: "
        "{}",
        j.dump(2),
        roundTripJSON.dump(2));
      OPENKNEEBOARD_BREAK;
      parseResult = std::unexpected {"JSON <=> Plugin had lossy round-trip."};
    }
  } catch (const nlohmann::json::exception& e) {
    parseResult = std::unexpected {
      std::format("Couldn't parse metadata file: {} ({})", e.what(), e.id)};
  }
  if (parseResult.has_value()) {
    co_await InstallPlugin(kneeboard, xamlRoot, path, *parseResult, zip.get());
  } else {
    co_await ShowPluginInstallationError(xamlRoot, path, parseResult.error());
    co_return;
  }
}

task<void> InstallPlugin(
  std::weak_ptr<KneeboardState> kneeboard,
  XamlRoot xamlRoot,
  const wchar_t* const commandLine) {
  int argc {};
  const wil::unique_hlocal_ptr<PWSTR[]> argv {
    CommandLineToArgvW(commandLine, &argc)};

  for (int i = 0; i < argc; ++i) {
    const std::wstring_view arg {argv[i]};
    if (arg != L"--plugin") {
      continue;
    }
    if (i == argc - 1) {
      dprint.Error("`--plugin` passed, but no plugin specified.");
      OPENKNEEBOARD_BREAK;
      co_return;
    }
    co_await InstallPluginFromPath(kneeboard, xamlRoot, argv[i + 1]);
    co_return;
  }

  co_return;
}
}// namespace OpenKneeboard