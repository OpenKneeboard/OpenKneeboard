// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenKneeboard/numeric_cast.hpp"

#include <OpenKneeboard/DirectInputDevice.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>
#include <OpenKneeboard/UserInputButtonEvent.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

#include <format>

namespace OpenKneeboard {

namespace {
enum class DirectInputButtonType : uint64_t {
  Button = 0,
  Key = 0,
  HatDirection = 1,
  VScroll = 2,
};

static constexpr auto DirectInputButtonTypeMask = (~(0ui64)) << 48;
static constexpr auto DirectInputButtonValueMask = ~DirectInputButtonTypeMask;

DirectInputButtonType GetButtonType(uint64_t button) {
  return static_cast<DirectInputButtonType>(button >> 48);
}

uint64_t EncodeButtonType(DirectInputButtonType buttonType) {
  return static_cast<uint64_t>(buttonType) << 48;
}

struct DirectInputHat {
  uint8_t mHat;
  uint16_t mValue;
};

DirectInputHat DecodeHat(uint64_t button) {
  winrt::check_bool(
    GetButtonType(button) == DirectInputButtonType::HatDirection);
  return {
    .mHat = static_cast<uint8_t>((button >> 32) & 0xff),
    .mValue = static_cast<uint16_t>(button & 0xffff),
  };
}

uint64_t EncodeHat(uint8_t hat, uint16_t value) {
  return EncodeButtonType(DirectInputButtonType::HatDirection)
    | (static_cast<uint64_t>(hat) << 32) | value;
}

uint64_t EncodeVScroll(DirectInputDevice::VScrollDirection direction) {
  return EncodeButtonType(DirectInputButtonType::VScroll)
    | static_cast<uint64_t>(direction);
}

DirectInputDevice::VScrollDirection DecodeVScroll(uint64_t button) {
  winrt::check_bool(GetButtonType(button) == DirectInputButtonType::VScroll);
  return static_cast<DirectInputDevice::VScrollDirection>(
    button & DirectInputButtonValueMask);
}

};// namespace

std::shared_ptr<DirectInputDevice> DirectInputDevice::Create(
  const DIDEVICEINSTANCEW& instance) {
  // Can't use make_shared because ctor is private
  return std::shared_ptr<DirectInputDevice>(new DirectInputDevice(instance));
}

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
  if ((mDevice.dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
    return this->GetKeyLabel(button);
  }

  const auto buttonType = GetButtonType(button);
  switch (buttonType) {
    case DirectInputButtonType::Button:
      return std::format("Button {}", button + 1);
    case DirectInputButtonType::VScroll: {
      const auto direction = DecodeVScroll(button);
      switch (direction) {
        case VScrollDirection::Up:
          return _("Wheel Up");
        case VScrollDirection::Down:
          return _("Wheel Down");
      }
    }
    case DirectInputButtonType::HatDirection: {
      const auto hat = DecodeHat(button);
      std::string value;
      switch (hat.mValue) {
        case 0:
          value = "N";
          break;
        case 4500:
          value = "NE";
          break;
        case 9000:
          value = "E";
          break;
        case 13500:
          value = "SE";
          break;
        case 18000:
          value = "S";
          break;
        case 22500:
          value = "SW";
          break;
        case 27000:
          value = "W";
          break;
        case 31500:
          value = "NW";
          break;
        default:
          value = std::format("{}°", hat.mValue / 100);
          break;
      }
      return std::format(_("Hat {} {}"), hat.mHat + 1, value);
    };
  }

  dprint("Unable to resolve label for DI button {:#016x}", button);
  OPENKNEEBOARD_BREAK;
  return std::format("INVALID {:#016x}", button);
}

std::string DirectInputDevice::GetKeyLabel(uint64_t key) const {
  switch (key) {
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
      return "LShift";
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
      return "RShift";
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
    case 0x67:
      return "F16";
    case 0x68:
      return "F17";
    case 0x69:
      return "F18";
    case 0x6a:
      return "F19";
    case 0x6b:
      return "F20";
    case 0x6c:
      return "F21";
    case 0x6d:
      return "F22";
    case 0x6e:
      return "F23";
    case 0x76:
      return "F24";
    default:
      return std::format("{:#x}", key);
  }
}

std::string DirectInputDevice::GetButtonComboDescription(
  const std::unordered_set<uint64_t>& ids) const {
  if (ids.empty()) {
    return _("None");
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

void DirectInputDevice::PostButtonStateChange(uint8_t id, bool pressed) {
  evButtonEvent.Emit(
    UserInputButtonEvent {
      this->shared_from_this(),
      id,
      pressed,
    });
}

void DirectInputDevice::PostVScroll(
  DirectInputDevice::VScrollDirection direction) {
  const auto button = EncodeVScroll(direction);
  evButtonEvent.Emit(
    UserInputButtonEvent {
      this->shared_from_this(),
      button,
      /* pressed = */ true,
    });
  evButtonEvent.Emit(
    UserInputButtonEvent {
      this->shared_from_this(),
      button,
      /* pressed = */ false,
    });
}

void DirectInputDevice::PostHatStateChange(
  uint8_t hat,
  DWORD oldValue,
  DWORD newValue) {
  // Center is documented as '-1', but DWORD is an unsigned type.
  // This is interpreted differently by different devices/drivers/...
  // so normalize.
  //
  // Non-center values are centidegrees, so max out at 35999, which is < 0xffff,
  // so we can losslesly normalize by truncating to 16 bits.
  constexpr uint32_t center = 0xffff;
  oldValue &= 0xffff;
  newValue &= 0xffff;

  if (oldValue != center) {
    evButtonEvent.Emit(
      UserInputButtonEvent {
        this->shared_from_this(),
        EncodeHat(hat, numeric_cast<uint16_t>(oldValue)),
        false,
      });
  }

  if (newValue != center) {
    evButtonEvent.Emit(
      UserInputButtonEvent {
        this->shared_from_this(),
        EncodeHat(hat, numeric_cast<uint16_t>(newValue)),
        true,
      });
  }
}

DIDEVICEINSTANCEW DirectInputDevice::GetDIDeviceInstance() const {
  return mDevice;
}

}// namespace OpenKneeboard
