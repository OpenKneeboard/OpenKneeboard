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

#include <OpenKneeboard/WintabTablet.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/utf8.h>

// clang-format off
#include <wintab/WINTAB.H>
#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE 0
#define PACKETEXPKEYS PKEXT_ABSOLUTE
#include <wintab/PKTDEF.H>
// clang-format on

#define WINTAB_FUNCTIONS \
  IT(WTInfoW) \
  IT(WTOpenW) \
  IT(WTGetW) \
  IT(WTClose) \
  IT(WTPacket)

namespace {
class LibWintab {
 public:
#define IT(x) decltype(&x) x = nullptr;
  WINTAB_FUNCTIONS
#undef IT

  LibWintab() {
    mWintab = LoadLibraryW(L"WINTAB32.dll");
    if (!mWintab) {
      return;
    }

#define IT(x) \
  this->x = reinterpret_cast<decltype(&::x)>(GetProcAddress(mWintab, #x));
    WINTAB_FUNCTIONS
#undef IT
  }

  ~LibWintab() {
    if (mWintab) {
      FreeLibrary(mWintab);
    }
  }

  operator bool() const {
    return mWintab != 0;
  }

  LibWintab(const LibWintab&) = delete;
  LibWintab& operator=(const LibWintab&) = delete;

 private:
  HMODULE mWintab = 0;
};
}// namespace

namespace OpenKneeboard {

struct WintabTablet::Impl {
  Impl(HWND window);
  ~Impl();

  State mState {};
  Limits mLimits {};
  LibWintab mWintab {};
  HCTX mCtx {nullptr};
};

WintabTablet::WintabTablet(HWND window) : p(std::make_unique<Impl>(window)) {
}

WintabTablet::~WintabTablet() {
}

WintabTablet::State WintabTablet::GetState() const {
  if (!p) {
    return {};
  }
  return p->mState;
}

WintabTablet::Limits WintabTablet::GetLimits() const {
  if (!p) {
    return {};
  }
  return p->mLimits;
};

bool WintabTablet::IsValid() const {
  return p && p->mCtx;
}

WintabTablet::Impl::Impl(HWND window) {
  if (!mWintab) {
    return;
  }

  LOGCONTEXT logicalContext;
  mWintab.WTInfoW(WTI_DEFSYSCTX, 0, &logicalContext);
  logicalContext.lcPktData = PACKETDATA;
  logicalContext.lcMoveMask = PACKETDATA;
  logicalContext.lcPktMode = PACKETMODE;
  logicalContext.lcOptions = CXO_MESSAGES;
  logicalContext.lcBtnDnMask = ~0;
  logicalContext.lcBtnUpMask = ~0;

  AXIS axis;

  mWintab.WTInfoW(WTI_DEVICES, DVC_X, &axis);
  logicalContext.lcInOrgX = axis.axMin;
  logicalContext.lcInExtX = axis.axMax - axis.axMin;
  logicalContext.lcOutOrgX = 0;
  // From
  // https://github.com/Wacom-Developer/wacom-device-kit-windows/blob/d2bc78fe79d442a3d398f750357e46effbca1daa/Wintab%20CAD%20Test/SampleCode/CadTest.cpp#L223-L231
  //
  // This prevents outputted display-tablet coordinates
  // range from being mapped to full desktop, which
  // causes problems in multi-screen set-ups. Ie, without this
  // then the tablet coord. range is mapped to full desktop, intead
  // of only the display tablet active area.
  logicalContext.lcOutExtX = axis.axMax - axis.axMin + 2;
  ;

  mWintab.WTInfoW(WTI_DEVICES, DVC_Y, &axis);
  logicalContext.lcInOrgY = axis.axMin;
  logicalContext.lcInExtY = axis.axMax - axis.axMin;
  logicalContext.lcOutOrgY = 0;
  // same trick as above
  logicalContext.lcOutExtY = -(axis.axMax - axis.axMin + 1);

  mWintab.WTInfoW(WTI_DEVICES, DVC_NPRESSURE, &axis);

  mLimits = {
    .x = static_cast<uint32_t>(logicalContext.lcOutExtX),
    .y = static_cast<uint32_t>(-logicalContext.lcOutExtY),
    .pressure = static_cast<uint32_t>(axis.axMax),
  };

  for (UINT i = 0, tag; mWintab.WTInfo(WTI_EXTENSIONS + i, EXT_TAG, &tag);
       ++i) {
    if (tag == WTX_EXPKEYS2 || tag == WTX_OBT) {
      UINT mask = 0;
      mWintab.WTInfo(WTI_EXTENSIONS + i, EXT_MASK, &mask);
      logicalContext.lcPktData |= mask;
      break;
    }
  }

  mCtx = mWintab.WTOpen(window, &logicalContext, true);
  if (!mCtx) {
    dprint("Failed to open wintab tablet");
    return;
  }
  dprint("Opened wintab tablet");
}

std::string WintabTablet::GetDeviceName() const {
  if (!p->mWintab) {
    return {};
  }

  std::wstring buf;
  buf.resize(
    p->mWintab.WTInfoW(WTI_DEVICES, DVC_NAME, nullptr) / sizeof(wchar_t));
  const auto actualSize = p->mWintab.WTInfoW(WTI_DEVICES, DVC_NAME, buf.data());
  buf.resize((actualSize / sizeof(wchar_t)) - 1);
  return to_utf8(buf);
}

std::string WintabTablet::GetDeviceID() const {
  if (!p->mWintab) {
    return {};
  }

  std::wstring buf;
  buf.resize(
    p->mWintab.WTInfoW(WTI_DEVICES, DVC_PNPID, nullptr) / sizeof(wchar_t));
  const auto actualSize
    = p->mWintab.WTInfoW(WTI_DEVICES, DVC_PNPID, buf.data());
  buf.resize((actualSize / sizeof(wchar_t)) - 1);
  return to_utf8(buf);
}

WintabTablet::Impl::~Impl() {
  if (!mCtx) {
    return;
  }
  mWintab.WTClose(mCtx);
}

bool WintabTablet::CanProcessMessage(UINT message) const {
  return message == WT_PROXIMITY || message == WT_PACKET
    || message == WT_PACKETEXT;
}

bool WintabTablet::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WT_PROXIMITY) {
    // high word indicates hardware events, low word indicates
    // context enter/leave
    p->mState.active = lParam & 0xffff;
    return true;
  }

  // Use the context from the param instead of our stored context so we
  // can also handle messages forwarded from another window/process,
  // e.g. the game :)

  if (message == WT_PACKET) {
    PACKET packet;
    auto ctx = reinterpret_cast<HCTX>(lParam);
    if (!p->mWintab.WTPacket(ctx, static_cast<UINT>(wParam), &packet)) {
      return false;
    }
    p->mState.x = packet.pkX;
    p->mState.y = packet.pkY;
    p->mState.pressure = packet.pkNormalPressure;
    p->mState.penButtons = static_cast<uint16_t>(packet.pkButtons);
    return true;
  }

  if (message == WT_PACKETEXT) {
    PACKETEXT packet;
    auto ctx = reinterpret_cast<HCTX>(lParam);
    if (!p->mWintab.WTPacket(ctx, static_cast<UINT>(wParam), &packet)) {
      return false;
    }
    const uint16_t mask = 1ui16 << packet.pkExpKeys.nControl;
    if (packet.pkExpKeys.nState) {
      p->mState.tabletButtons |= mask;
    } else {
      p->mState.tabletButtons &= ~mask;
    }
    return true;
  }

  return false;
}

}// namespace OpenKneeboard
