// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <charconv>
#include <chrono>
#include <string_view>

static uint32_t hex_to_ui32(const std::string_view& sv) {
  if (sv.empty()) {
    return 0;
  }

  uint32_t value = 0;
  std::from_chars(&sv.front(), &sv.front() + sv.length(), value, 16);
  return value;
}

auto& MailslotHandle() {
  static winrt::file_handle sHandle;
  return sHandle;
}

static bool OpenMailslotHandle() {
  if (MailslotHandle()) {
    return true;
  }

  static std::chrono::steady_clock::time_point sLastAttempt {};
  const auto now = std::chrono::steady_clock::now();
  if (now - sLastAttempt < std::chrono::seconds(1)) {
    return false;
  }
  sLastAttempt = now;

  MailslotHandle() = OpenKneeboard::Win32::or_default::CreateFile(
    OpenKneeboard::APIEvent::GetMailslotPath(),
    GENERIC_WRITE,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    0,
    NULL);

  return static_cast<bool>(MailslotHandle());
}

namespace OpenKneeboard {

#define CHECK_PACKET(condition) \
  if (!(condition)) { \
    dprint("Check failed at {}:{}: {}", __FILE__, __LINE__, #condition); \
    return {}; \
  }

APIEvent::operator bool() const { return !(name.empty() || value.empty()); }

APIEvent APIEvent::Unserialize(std::string_view packet) {
  // "{:08x}!{}!{:08x}!{}!", name size, name, value size, value
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

std::vector<std::byte> APIEvent::Serialize() const {
  const auto str =
    std::format("{:08x}!{}!{:08x}!{}!", name.size(), name, value.size(), value);
  const auto first = reinterpret_cast<const std::byte*>(str.data());
  return {first, first + str.size()};
}

void APIEvent::Send() const {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity,
    "APIEvent::Send()",
    TraceLoggingValue(name.c_str(), "Name"),
    TraceLoggingBinary(value.c_str(), value.size(), "Value"));

  if (!OpenMailslotHandle()) {
    TraceLoggingWriteStop(
      activity,
      "APIEvent::Send()",
      TraceLoggingValue("Couldn't open mailslot", "Result"));
    return;
  }
  const auto packet = this->Serialize();

  if (WriteFile(
        MailslotHandle().get(),
        packet.data(),
        static_cast<DWORD>(packet.size()),
        nullptr,
        nullptr)) {
    TraceLoggingWriteStop(
      activity, "APIEvent::Send()", TraceLoggingValue("Success", "Result"));
    return;
  }

  MailslotHandle().close();
  MailslotHandle() = {};
  TraceLoggingWriteTagged(activity, "Closed handle");

  if (!OpenMailslotHandle()) {
    TraceLoggingWriteStop(
      activity,
      "APIEvent::Send()",
      TraceLoggingValue("Couldn't reopen handle", "Result"));
    return;
  }
  TraceLoggingWriteTagged(activity, "Reopened handle");
  if (WriteFile(
        MailslotHandle().get(),
        packet.data(),
        static_cast<DWORD>(packet.size()),
        nullptr,
        nullptr)) {
    TraceLoggingWriteStop(
      activity, "APIEvent::Send()", TraceLoggingValue("Success", "Result"));
  } else {
    TraceLoggingWriteStop(
      activity,
      "APIEvent::Send()",
      TraceLoggingValue("Error", "Result"),
      TraceLoggingValue(GetLastError(), "Error"));
  }
}

const wchar_t* APIEvent::GetMailslotPath() {
  static std::wstring sPath;
  if (sPath.empty()) {
    sPath = std::format(
      L"\\\\.\\mailslot\\{}.events.v1.3", OpenKneeboard::ProjectReverseDomainW);
  }
  return sPath.c_str();
}

OPENKNEEBOARD_DEFINE_JSON(SetTabByIDEvent, mID, mPageNumber, mKneeboard);
OPENKNEEBOARD_DEFINE_JSON(SetTabByNameEvent, mName, mPageNumber, mKneeboard);
OPENKNEEBOARD_DEFINE_JSON(SetTabByIndexEvent, mIndex, mPageNumber, mKneeboard);
OPENKNEEBOARD_DEFINE_JSON(SetProfileByGUIDEvent, mGUID);
OPENKNEEBOARD_DEFINE_JSON(SetProfileByNameEvent, mName);

NLOHMANN_JSON_SERIALIZE_ENUM(
  SetBrightnessEvent::Mode,
  {
    {SetBrightnessEvent::Mode::Absolute, "Absolute"},
    {SetBrightnessEvent::Mode::Relative, "Relative"},
  });
OPENKNEEBOARD_DEFINE_JSON(SetBrightnessEvent, mBrightness, mMode);

OPENKNEEBOARD_DEFINE_JSON(PluginTabCustomActionEvent, mActionID, mExtraData);

}// namespace OpenKneeboard
