// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>

using OpenKneeboard::APIEvent;

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>

#include <filesystem>

#include <shellapi.h>

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  int argc = 0;
  auto argv = CommandLineToArgvW(pCmdLine, &argc);

  int repeat = 1;

  wchar_t* wpgmptr {nullptr};
  _get_wpgmptr(&wpgmptr);
  // argv[0] is sometimes the first arg, sometimes the program name
  if (
    std::filesystem::weakly_canonical(argv[0])
    != std::filesystem::weakly_canonical(wpgmptr)) {
    try {
      repeat = std::stoi(argv[0]);
    } catch (...) {
    }
  }

  if (repeat == 1) {
    const APIEvent ge {
      APIEvent::EVT_REMOTE_USER_ACTION,
      STRINGIFY(REMOTE_ACTION),
    };
    ge.Send();
    return 0;
  }

  auto payload = nlohmann::json::array();
  for (int i = 0; i < repeat; ++i) {
    payload.push_back(nlohmann::json::array(
      {APIEvent::EVT_REMOTE_USER_ACTION, STRINGIFY(REMOTE_ACTION)}));
  }
  const APIEvent me {
    APIEvent::EVT_MULTI_EVENT,
    payload.dump(),
  };
  me.Send();
  return 0;
}
