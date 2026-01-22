// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/BrowserTab.hpp>
#include <OpenKneeboard/DCSAircraftTab.hpp>
#include <OpenKneeboard/DCSBriefingTab.hpp>
#include <OpenKneeboard/DCSMissionTab.hpp>
#include <OpenKneeboard/DCSRadioLogTab.hpp>
#include <OpenKneeboard/DCSTerrainTab.hpp>
#include <OpenKneeboard/EndlessNotebookTab.hpp>
#include <OpenKneeboard/FolderTab.hpp>
#include <OpenKneeboard/SingleFileTab.hpp>
#include <OpenKneeboard/WindowCaptureTab.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <concepts>

#define OPENKNEEBOARD_TAB_TYPES \
  IT(_("Files (one per tab)"), SingleFile) \
  IT(_("Folder"), Folder) \
  IT(_("Endless Notebook (from template file)"), EndlessNotebook) \
  IT(_("Window Capture"), WindowCapture) \
  IT(_("Web Dashboard"), Browser) \
  IT(_("DCS Aircraft Kneeboard"), DCSAircraft) \
  IT(_("DCS Mission Briefing"), DCSBriefing) \
  IT(_("DCS Mission Kneeboard"), DCSMission) \
  IT(_("DCS Radio Log"), DCSRadioLog) \
  IT(_("DCS Theater Kneeboard"), DCSTerrain)

// If this fails, check that you included the header :)
#define IT(_, type) \
  static_assert( \
    std::derived_from<OpenKneeboard::type##Tab, OpenKneeboard::ITab>);
OPENKNEEBOARD_TAB_TYPES
#undef IT

namespace OpenKneeboard {

enum {
#define IT(_, key) TABTYPE_IDX_##key,
  OPENKNEEBOARD_TAB_TYPES
#undef IT
};

enum class TabType {
#define IT(_, key) key = TABTYPE_IDX_##key,
  OPENKNEEBOARD_TAB_TYPES
#undef IT
};

struct DXResources;

/** Uniform construction classes, regardless of how their memory is managed.
 *
 * - `new Foo(args...)`: self explanatory
 * - `Foo::Create(args...)`: generally used in combination with
 *   `std::enable_shared_from_this`.
 *
 * In traditional code, `std::enable_shared_from_this` is usually an
 * anti-pattern, but when async callbacks are involved, it's essential
 * for safety; callbacks should capture a weak_ptr and abort if it can't be
 * locked.
 */
namespace detail {
template <class T, class... TArgs>
  requires std::constructible_from<T, TArgs...>
task<std::shared_ptr<T>> make_shared(TArgs&&... args) {
  co_return std::make_shared<T>(std::forward<TArgs>(args)...);
}

template <class T, class... TArgs>
  requires requires(TArgs... args) {
    { T::Create(args...) } -> std::same_as<std::shared_ptr<T>>;
  }
task<std::shared_ptr<T>> make_shared(TArgs&&... args) {
  co_return T::Create(std::forward<TArgs>(args)...);
}
template <class T, class... TArgs>
  requires requires(TArgs... args) {
    { T::Create(args...) } -> std::convertible_to<task<std::shared_ptr<T>>>;
  }
task<std::shared_ptr<T>> make_shared(TArgs&&... args) {
  return T::Create(std::forward<TArgs>(args)...);
}

template <class T, class... TArgs>
concept shared_constructible_from = requires(TArgs... args) {
  { detail::make_shared<T>(args...) } -> std::same_as<task<std::shared_ptr<T>>>;
};

}// namespace detail

// Public constructor
static_assert(detail::shared_constructible_from<
              SingleFileTab,
              const audited_ptr<DXResources>&,
              KneeboardState*,
              const std::filesystem::path&>);
static_assert(!detail::shared_constructible_from<
              SingleFileTab,
              const audited_ptr<DXResources>&,
              KneeboardState*>);
// Static method
static_assert(detail::shared_constructible_from<
              WindowCaptureTab,
              const audited_ptr<DXResources>&,
              KneeboardState*,
              const winrt::guid&,
              const std::string&,
              const nlohmann::json&>);

/** Create a `shared_ptr<ITab>` with existing config */
template <std::derived_from<ITab> T>
task<std::shared_ptr<T>> load_tab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  const std::string& title,
  const nlohmann::json& config) {
  static_assert(!std::is_abstract_v<T>);

  if constexpr (std::derived_from<T, ITabWithSettings>) {
    typename TabSettingsTraits<T>::Settings settings;
    settings = config;
    return detail::make_shared<T>(dxr, kbs, persistentID, title, settings);
  } else {
    return detail::make_shared<T>(dxr, kbs, persistentID, title);
  }
}

template <class T>
concept loadable_tab = std::invocable<
  // FIXME: check return type too
  decltype(&load_tab<T>),
  const audited_ptr<DXResources>&,
  KneeboardState*,
  const winrt::guid&,
  const std::string&,
  const nlohmann::json&>;

#define IT(_, T) \
  static_assert(loadable_tab<T##Tab>, "Couldn't construct " #T "Tab");
OPENKNEEBOARD_TAB_TYPES
#undef IT

}// namespace OpenKneeboard
