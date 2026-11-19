// OpenKneeboard
//
// Copyright (c) 2026 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "OpenKneeboard/CursorEvent.hpp"
#include "OpenKneeboard/Events.hpp"

#include <Windows.h>

#include <felly/non_copyable.hpp>

#include <array>

namespace OpenKneeboard {

class KneeboardState;

class GlobalMouseHook final : public felly::non_copyable {
 public:
  GlobalMouseHook() = delete;
  ~GlobalMouseHook();

  static GlobalMouseHook Get(KneeboardState*);

  GlobalMouseHook(GlobalMouseHook&& other) noexcept { MoveFrom(other); };
  GlobalMouseHook& operator=(GlobalMouseHook&& other) noexcept {
    MoveFrom(other);
    return *this;
  }

 private:
  static LRESULT CALLBACK
  MouseCallback(int nCode, WPARAM wParam, LPARAM lParam) noexcept;
  static LRESULT CALLBACK
  KeyboardCallback(int nCode, WPARAM wParam, LPARAM lParam) noexcept;

  explicit GlobalMouseHook(KneeboardState*);

  KneeboardState* mKneeboard {};
  HHOOK mMouseHook {nullptr};
  HHOOK mKeyboardHook {nullptr};

  POINT mOrigin {};
  uint32_t mButtons {};

  std::array<LARGE_INTEGER, 5> mEscapeTimestamps {};

  void OnMouseMessage(WPARAM message, MSLLHOOKSTRUCT*);
  void OnKeyboardMessage(WPARAM message, KBDLLHOOKSTRUCT*);

  void MoveFrom(GlobalMouseHook& other) noexcept;
  void Unhook() noexcept;
};

}// namespace OpenKneeboard
