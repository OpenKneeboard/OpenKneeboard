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

#include <OpenKneeboard/json_fwd.h>
#include <OpenKneeboard/utf8.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard {
struct GameEvent final {
  // These are both required to be UTF-8
  std::string name;
  std::string value;

  template <class T>
  T ParsedValue() const {
    if (name != T::ID) {
      throw std::logic_error("Parse type does not match event name");
    }
    return nlohmann::json::parse(this->value);
  }

  // TODO (C++23): std::expected with exception
  template <class T>
  std::optional<T> TryParsedValue() const {
    // Intentionally not propagating the std::logic_error
    try {
      return ParsedValue<T>();
    } catch (const nlohmann::json::exception& e) {
      return {};
    }
  }

  template <class T>
  static GameEvent FromStruct(const T& v) {
    nlohmann::json j;
    j = v;
    return {T::ID, j.dump()};
  }

  operator bool() const;

  static GameEvent Unserialize(std::string_view packet);
  std::vector<std::byte> Serialize() const;
  void Send() const;

  static const wchar_t* GetMailslotPath();

  /// String name of OpenKneeboard::UserAction enum member
  static constexpr char EVT_REMOTE_USER_ACTION[] = "RemoteUserAction";

  /// String KneeboardViewID
  static constexpr char EVT_SET_INPUT_FOCUS[] = "SetInputFocus";

  /// struct SetTabByIDEvent
  static constexpr char EVT_SET_TAB_BY_ID[] = "SetTabByID";
  /// struct SetTabByNameEvent
  static constexpr char EVT_SET_TAB_BY_NAME[] = "SetTabByName";
  /// struct SetTabByIndexEvent
  static constexpr char EVT_SET_TAB_BY_INDEX[] = "SetTabByIndex";

  /// struct SetProfileByIDEvent
  static constexpr char EVT_SET_PROFILE_BY_ID[] = "SetProfileByID";
  /// struct SetProfileByNameEvent
  static constexpr char EVT_SET_PROFILE_BY_NAME[] = "SetProfileByName";
  // struct SetBrightnessEvent
  static constexpr char EVT_SET_BRIGHTNESS[] = "SetBrightness";

  /// JSON: "[ [name, value], [name, value], ... ]"
  static constexpr char EVT_MULTI_EVENT[] = "MultiEvent";
};

struct BaseSetTabEvent {
  // 0 = no change
  uint64_t mPageNumber {0};
  // 0 = 'active', 1 = primary, 2 = secondary
  uint8_t mKneeboard {0};
};

struct SetTabByIDEvent : public BaseSetTabEvent {
  static constexpr auto ID {GameEvent::EVT_SET_TAB_BY_ID};
  std::string mID;
};
OPENKNEEBOARD_DECLARE_JSON(SetTabByIDEvent);

struct SetTabByNameEvent : public BaseSetTabEvent {
  static constexpr auto ID {GameEvent::EVT_SET_TAB_BY_NAME};
  std::string mName;
};
OPENKNEEBOARD_DECLARE_JSON(SetTabByNameEvent);

struct SetTabByIndexEvent : public BaseSetTabEvent {
  static constexpr auto ID {GameEvent::EVT_SET_TAB_BY_INDEX};
  uint64_t mIndex {};
};
OPENKNEEBOARD_DECLARE_JSON(SetTabByIndexEvent);

struct SetProfileByIDEvent {
  static constexpr auto ID {GameEvent::EVT_SET_PROFILE_BY_ID};
  std::string mID;
};
OPENKNEEBOARD_DECLARE_JSON(SetProfileByIDEvent);

struct SetProfileByNameEvent {
  static constexpr auto ID {GameEvent::EVT_SET_PROFILE_BY_NAME};
  std::string mName;
};
OPENKNEEBOARD_DECLARE_JSON(SetProfileByNameEvent);

struct SetBrightnessEvent {
  enum class Mode {
    Absolute,
    Relative,
  };
  static constexpr auto ID {GameEvent::EVT_SET_BRIGHTNESS};
  float mBrightness {};
  Mode mMode = Mode::Absolute;
};
OPENKNEEBOARD_DECLARE_JSON(SetBrightnessEvent);

}// namespace OpenKneeboard
