// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <Windows.h>

#include <memory>

namespace OpenKneeboard {

class ChromiumApp final {
 public:
  ChromiumApp() = delete;
  ChromiumApp(HINSTANCE);
  ~ChromiumApp();

  [[nodiscard]]
  int ExecuteSubprocess();

  void InitializeBrowserProcess();

 private:
  enum class State {
    Unused,
    Subprocess,
    SubprocessComplete,
    BrowserProcess,
  };
  State mState {State::Unused};
  // I usually dislike PImpl, but in this case, I don't want to expose
  // generically-named macros to things using this class, such as
  // IMPLEMENT_REFCOUNTING and DISALLOW_COPY_AND_ASSIGN
  struct Wrapper;
  class Impl;
  std::unique_ptr<Wrapper> p;
};

}// namespace OpenKneeboard