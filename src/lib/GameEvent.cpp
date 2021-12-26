#include "OpenKneeboard\GameEvent.h"

#include "OpenKneeboard/dprint.h"

#include <charconv>
#include <string_view>

using OpenKneeboard::dprintf;

static uint32_t hex_to_ui32(const std::string_view& sv) {
  if (sv.empty()) {
    return 0;
  }

  uint32_t value = 0;
  std::from_chars(&sv.front(), &sv.front() + sv.length(), value, 16);
  return value;
}

namespace OpenKneeboard {

#define CHECK_PACKET(condition) \
  if (!(condition)) { \
    dprintf("Check failed at {}:{}: {}", __FILE__, __LINE__, #condition); \
    return {}; \
  }

GameEvent::operator bool() const {
  return !(Name.empty() || Value.empty());
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
  const auto name = packet.substr(nameOffset, nameLen);

  const uint32_t valueLenOffset = nameOffset + nameLen + 1;
  CHECK_PACKET(packet.size() >= valueLenOffset + 10);
  const auto valueLen = hex_to_ui32(packet.substr(valueLenOffset, 8));
  const uint32_t valueOffset = valueLenOffset + 8 + 1;
  CHECK_PACKET(packet.size() == valueOffset + valueLen + 1);
  const auto value = packet.substr(valueOffset, valueLen);

  return {.Name = std::string(name), .Value = std::string(value)};
}

std::vector<std::byte> GameEvent::Serialize() const {
  const auto str = fmt::format(
    "{:08x}!{}!{:08x}!{}!", Name.size(), Name, Value.size(), Value);
  const auto first = reinterpret_cast<const std::byte*>(str.data());
  return { first, first + str.size() };
}

}// namespace OpenKneeboard
