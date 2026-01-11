// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "simple-remotes.hpp"

namespace OpenKneeboard::SimpleRemotes {

int main(const UserAction action, const Args& args) {
  static const std::string ActionString {magic_enum::enum_name(action)};

  const auto count = *args.mCount;

  dprint("Remote invoked: {} (count: {})", ActionString, count);

  switch (count) {
    case 0:
      return EXIT_SUCCESS;
    case 1:
      APIEvent {APIEvent::EVT_REMOTE_USER_ACTION, ActionString}.Send();
      return EXIT_SUCCESS;
    default: {
      const auto single = nlohmann::json::array(
        {APIEvent::EVT_REMOTE_USER_ACTION, ActionString});

      auto payload = nlohmann::json::array();
      for (std::size_t i = 0; i < count; ++i) {
        payload.push_back(single);
      }
      APIEvent {APIEvent::EVT_MULTI_EVENT, payload.dump()}.Send();
      return EXIT_SUCCESS;
    }
  }
}

}// namespace OpenKneeboard::SimpleRemotes