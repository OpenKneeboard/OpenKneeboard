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

#include <OpenKneeboard/DirectInputDevice.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <shims/winrt.h>
#include <shims/wx.h>

namespace OpenKneeboard {

DirectInputDevice::DirectInputDevice(const DIDEVICEINSTANCEW& device)
  : mDevice(device) {
}

std::string DirectInputDevice::GetName() const {
  return to_utf8(mDevice.tszInstanceName);
}

std::string DirectInputDevice::GetID() const {
  return to_utf8(winrt::to_hstring(mDevice.guidInstance));
}

std::string DirectInputDevice::GetButtonLabel(uint64_t button) const {
  if ((mDevice.dwDevType & 0xff) != DI8DEVTYPE_KEYBOARD) {
    return fmt::to_string(button + 1);
  }
  switch (button) {
    case DIK_ESCAPE:
      return "Esc";
    case DIK_1:
      return "1";
    case DIK_2:
      return "2";
    case DIK_3:
      return "3";
    case DIK_4:
      return "4";
    case DIK_5:
      return "5";
    case DIK_6:
      return "6";
    case DIK_7:
      return "7";
    case DIK_8:
      return "8";
    case DIK_9:
      return "9";
    case DIK_0:
      return "0";
    case DIK_MINUS:
      return "-";
    case DIK_EQUALS:
      return "=";
    case DIK_BACK:
      return "Backspace";
    case DIK_TAB:
      return "Tab";
    case DIK_Q:
      return "Q";
    case DIK_W:
      return "W";
    case DIK_E:
      return "E";
    case DIK_R:
      return "R";
    case DIK_T:
      return "T";
    case DIK_Y:
      return "Y";
    case DIK_U:
      return "U";
    case DIK_I:
      return "I";
    case DIK_O:
      return "O";
    case DIK_P:
      return "P";
    case DIK_LBRACKET:
      return "[";
    case DIK_RBRACKET:
      return "]";
    case DIK_RETURN:
      return "Return";
    case DIK_LCONTROL:
      return "LCtrl";
    case DIK_A:
      return "A";
    case DIK_S:
      return "S";
    case DIK_D:
      return "S";
    case DIK_F:
      return "F";
    case DIK_G:
      return "G";
    case DIK_H:
      return "H";
    case DIK_J:
      return "J";
    case DIK_K:
      return "K";
    case DIK_L:
      return "L";
    case DIK_SEMICOLON:
      return ";";
    case DIK_APOSTROPHE:
      return "'";
    case DIK_GRAVE:
      return "`";
    case DIK_LSHIFT:
      return "L⇧";
    case DIK_BACKSLASH:
      return "\\";
    case DIK_Z:
      return "Z";
    case DIK_X:
      return "X";
    case DIK_C:
      return "C";
    case DIK_V:
      return "V";
    case DIK_B:
      return "B";
    case DIK_N:
      return "N";
    case DIK_M:
      return "M";
    case DIK_COMMA:
      return ",";
    case DIK_PERIOD:
      return ".";
    case DIK_SLASH:
      return "/";
    case DIK_RSHIFT:
      return "R⇧";
    case DIK_MULTIPLY:
      return "NP*";
    case DIK_LMENU:
      return "LAlt";
    case DIK_SPACE:
      return "Space";
    case DIK_CAPITAL:
      return "Caps";
    case DIK_F1:
      return "F1";
    case DIK_F2:
      return "F2";
    case DIK_F3:
      return "F3";
    case DIK_F4:
      return "F4";
    case DIK_F5:
      return "F5";
    case DIK_F6:
      return "F6";
    case DIK_F7:
      return "F7";
    case DIK_F8:
      return "F8";
    case DIK_F9:
      return "F9";
    case DIK_F10:
      return "F10";
    case DIK_NUMLOCK:
      return "NumLock";
    case DIK_SCROLL:
      return "ScrollLock";
    case DIK_NUMPAD7:
      return "NP7";
    case DIK_NUMPAD8:
      return "NP8";
    case DIK_NUMPAD9:
      return "NP9";
    case DIK_SUBTRACT:
      return "NP-";
    case DIK_NUMPAD4:
      return "NP4";
    case DIK_NUMPAD5:
      return "NP5";
    case DIK_NUMPAD6:
      return "NP6";
    case DIK_ADD:
      return "NP+";
    case DIK_NUMPAD1:
      return "NP1";
    case DIK_NUMPAD2:
      return "NP2";
    case DIK_NUMPAD3:
      return "NP3";
    case DIK_NUMPAD0:
      return "NP0";
    case DIK_DECIMAL:
      return "NP.";
    // case DIK_OEM_102:
    case DIK_F11:
      return "F11";
    case DIK_F12:
      return "F12";
    case DIK_F13:
      return "F13";
    case DIK_F14:
      return "F14";
    case DIK_F15:
      return "F15";
    case DIK_NUMPADEQUALS:
      return "NP=";
    case DIK_NUMPADENTER:
      return "NPEnter";
    case DIK_RCONTROL:
      return "RCtrl";
    case DIK_DIVIDE:
      return "NP/";
    case DIK_RMENU:
      return "RAlt";
    case DIK_PAUSE:
      return "Pause";
    case DIK_HOME:
      return "Home";
    case DIK_UP:
      return "↑";
    case DIK_PRIOR:
      return "PgUp";
    case DIK_LEFT:
      return "←";
    case DIK_RIGHT:
      return "→";
    case DIK_END:
      return "End";
    case DIK_DOWN:
      return "↓";
    case DIK_NEXT:
      return "PgDn";
    case DIK_INSERT:
      return "Insert";
    case DIK_DELETE:
      return "Delete";
    default:
      return fmt::format("{:#x}", button);
  }
}

std::string DirectInputDevice::GetButtonComboDescription(
  const std::unordered_set<uint64_t>& ids) const {
  if (ids.empty()) {
    return _("None").utf8_string();
  }
  if (ids.size() == 1) {
    auto label = (mDevice.dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD
      ? "{}"
      : _("Button {}").utf8_string();
    return fmt::format(fmt::runtime(label), GetButtonLabel(*ids.begin()));
  }
  std::string out;
  for (auto id: ids) {
    if (!out.empty()) {
      out += " + ";
    }
    out += GetButtonLabel(id);
  }
  return out;
}

std::vector<UserInputButtonBinding> DirectInputDevice::GetButtonBindings()
  const {
  return mButtonBindings;
}

void DirectInputDevice::SetButtonBindings(
  const std::vector<UserInputButtonBinding>& bindings) {
  mButtonBindings = bindings;
  evBindingsChangedEvent.Emit();
}

DIDEVICEINSTANCEW DirectInputDevice::GetDIDeviceInstance() const {
  return mDevice;
}

}// namespace OpenKneeboard
