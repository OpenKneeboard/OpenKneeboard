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
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/json.h>
#include <Windows.h>
#include <shims/winrt/base.h>

#include <charconv>
#include <string_view>

static uint32_t hex_to_ui32(const std::string_view& sv) {
  if (sv.empty()) {
    return 0;
  }

  uint32_t value = 0;
  std::from_chars(&sv.front(), &sv.front() + sv.length(), value, 16);
  return value;
}

static winrt::file_handle gMailslotHandle;

static bool OpenMailslotHandle() {
  if (!gMailslotHandle) {
    gMailslotHandle = {CreateFileA(
      OpenKneeboard::GameEvent::GetMailslotPath(),
      GENERIC_WRITE,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      0,
      NULL)};
  }
  return (bool)gMailslotHandle;
}

namespace OpenKneeboard {

#define CHECK_PACKET(condition) \
  if (!(condition)) { \
    dprintf("Check failed at {}:{}: {}", __FILE__, __LINE__, #condition); \
    return {}; \
  }

GameEvent::operator bool() const {
  return !(name.empty() || value.empty());
}

GameEvent GameEvent::Unserialize(const std::vector<std::byte>& buffer) {
  // "{:08x}!{}!{:08x}!{}!", name size, name, value size, value
  const std::string_view packet(
    reinterpret_cast<const char*>(buffer.data()), buffer.size());
  CHECK_PACKET(packet.ends_with("!"));
  CHECK_PACKET(packet.size() >= sizeof("12345678!!12345678!!") - 1);

  const auto nameLen = hex_to_ui32(packet.substr(0, 8));
  CHECK_PACKET(packet.size() >= 8 + nameLen + 8 + 4);
  const uint32_t nameOffset = 9;
  const std::string name(packet.substr(nameOffset, nameLen));

  const uint32_t valueLenOffset = nameOffset + nameLen + 1;
  CHECK_PACKET(packet.size() >= valueLenOffset + 10);
  const auto valueLen = hex_to_ui32(packet.substr(valueLenOffset, 8));
  const uint32_t valueOffset = valueLenOffset + 8 + 1;
  CHECK_PACKET(packet.size() == valueOffset + valueLen + 1);
  const std::string value(packet.substr(valueOffset, valueLen));

  return {name, value};
}

std::vector<std::byte> GameEvent::Serialize() const {
  const auto str = std::format(
    "{:08x}!{}!{:08x}!{}!", name.size(), name, value.size(), value);
  const auto first = reinterpret_cast<const std::byte*>(str.data());
  return {first, first + str.size()};
}

void GameEvent::Send() const {
  if (!OpenMailslotHandle()) {
    return;
  }
  const auto packet = this->Serialize();

  if (WriteFile(
        gMailslotHandle.get(),
        packet.data(),
        static_cast<DWORD>(packet.size()),
        nullptr,
        nullptr)) {
    return;
  }
  gMailslotHandle.close();
  gMailslotHandle = {};

  if (!OpenMailslotHandle()) {
    return;
  }
  WriteFile(
    gMailslotHandle.get(),
    packet.data(),
    static_cast<DWORD>(packet.size()),
    nullptr,
    nullptr);
}

const char* GameEvent::GetMailslotPath() {
  static std::string sPath;
  if (sPath.empty()) {
    sPath = std::format(
      "\\\\.\\mailslot\\{}.events.v1.3", OpenKneeboard::ProjectNameA);
  }
  return sPath.c_str();
}

OPENKNEEBOARD_DEFINE_JSON(SetTabByIDEvent, mID, mPageNumber, mKneeboard);
OPENKNEEBOARD_DEFINE_JSON(SetTabByNameEvent, mName, mPageNumber, mKneeboard);
OPENKNEEBOARD_DEFINE_JSON(SetTabByIndexEvent, mIndex, mPageNumber, mKneeboard);

}// namespace OpenKneeboard
