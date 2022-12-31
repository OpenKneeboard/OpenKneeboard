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

#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/DCSBriefingTab.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/EndlessNotebookTab.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/SingleFileTab.h>
#include <OpenKneeboard/WindowCaptureTab.h>
#include <shims/winrt/base.h>

#include <concepts>

#define OPENKNEEBOARD_TAB_TYPES \
  IT(_("Files (one per tab)"), SingleFile) \
  IT(_("Folder"), Folder) \
  IT(_("Window Capture"), WindowCapture) \
  IT(_("Endless Notebook (from template file)"), EndlessNotebook) \
  IT(_("DCS Aircraft Kneeboard"), DCSAircraft) \
  IT(_("DCS Mission Briefing"), DCSBriefing) \
  IT(_("DCS Mission Kneeboard"), DCSMission) \
  IT(_("DCS Radio Log"), DCSRadioLog) \
  IT(_("DCS Terrain Kneeboard"), DCSTerrain)

// If this fails, check that you included the header :)
#define IT(_, type) \
  static_assert( \
    std::derived_from<OpenKneeboard::type##Tab, OpenKneeboard::ITab>);
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
auto make_shared(TArgs&&... args) {
  return std::make_shared<T>(std::forward<TArgs>(args)...);
}

template <class T, class... TArgs>
  requires requires(TArgs... args) {
             { T::Create(args...) } -> std::same_as<std::shared_ptr<T>>;
           }
auto make_shared(TArgs&&... args) {
  return T::Create(std::forward<TArgs>(args)...);
}

template <class T, class... TArgs>
concept shared_constructible_from = requires(TArgs... args) {
                                      {
                                        detail::make_shared<T>(args...)
                                        } -> std::same_as<std::shared_ptr<T>>;
                                    };

}// namespace detail

// Public constructor
static_assert(detail::shared_constructible_from<
              SingleFileTab,
              const DXResources&,
              KneeboardState*,
              const std::filesystem::path&>);
// Static method
static_assert(detail::shared_constructible_from<
              WindowCaptureTab,
              const DXResources&,
              KneeboardState*,
              const winrt::guid&,
              const std::string&,
              const nlohmann::json&>);

/** Create a `shared_ptr<ITab>` with existing config */
template <std::derived_from<ITab> T>
std::shared_ptr<T> load_tab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  const std::string& title,
  const nlohmann::json& config) {
  if constexpr (detail::shared_constructible_from<
                  T,
                  DXResources,
                  KneeboardState*,
                  winrt::guid,
                  std::string,
                  nlohmann::json>) {
    return detail::make_shared<T>(dxr, kbs, persistentID, title, config);
  }

  if constexpr (detail::shared_constructible_from<
                  T,
                  DXResources,
                  KneeboardState*,
                  winrt::guid,
                  std::string>) {
    return detail::make_shared<T>(dxr, kbs, persistentID, title);
  }

  static_assert(!std::is_abstract_v<T>);
}

template <class T>
concept loadable_tab = std::is_invocable_r_v<
  std::shared_ptr<T>,
  decltype(&load_tab<T>),
  const DXResources&,
  KneeboardState*,
  const winrt::guid&,
  const std::string&,
  const nlohmann::json&>;

#define IT(_, T) static_assert(loadable_tab<T##Tab>);
OPENKNEEBOARD_TAB_TYPES
#undef IT

}// namespace OpenKneeboard
