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
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/utf8.h>

#include <stdexcept>
#include <thread>

// clang-format off
#include <wintab/WINTAB.H>
#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CHANGED)
#define PACKETMODE 0
#define PACKETEXPKEYS PKEXT_ABSOLUTE
#include <wintab/PKTDEF.H>
// clang-format on

#define WINTAB_FUNCTIONS \
  IT(WTInfoW) \
  IT(WTOpenW) \
  IT(WTClose) \
  IT(WTOverlap) \
  IT(WTPacket)

namespace OpenKneeboard {

namespace {

class LibWintab {
 public:
#define IT(x) decltype(&x) x = nullptr;
  WINTAB_FUNCTIONS
#undef IT

  LibWintab() : mWintab(LoadLibraryW(L"WINTAB32.dll")) {
    if (!mWintab) {
      return;
    }

#define IT(x) \
  this->x = reinterpret_cast<decltype(&::x)>(GetProcAddress(mWintab.get(), #x));
    WINTAB_FUNCTIONS
#undef IT
  }

  operator bool() const {
    return mWintab != 0;
  }

  LibWintab(const LibWintab&) = delete;
  LibWintab& operator=(const LibWintab&) = delete;

 private:
  unique_hmodule mWintab = 0;
};
}// namespace

static WintabTablet* gInstance {nullptr};

struct WintabTablet::Impl {
  Impl(HWND window);
  ~Impl();

  State mState {};
  Limits mLimits {};
  LibWintab mWintab {};
  HCTX mCtx {nullptr};

  std::jthread mOverlapThread;
  unique_hwineventhook mEventHook;
};

WintabTablet::WintabTablet(HWND window) : p(std::make_unique<Impl>(window)) {
  if (gInstance) {
    throw std::runtime_error("Only one WintabTablet at a time!");
  }
  gInstance = this;
}

WintabTablet::~WintabTablet() {
  gInstance = nullptr;
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

void CALLBACK WintabTablet::WinEventProc_SetOverlap(
  HWINEVENTHOOK hWinEventHook,
  DWORD event,
  HWND hwnd,
  LONG idObject,
  LONG idChild,
  DWORD idEventThread,
  DWORD dwmsEventTime) {
  if (!gInstance) {
    return;
  }

  gInstance->p->mOverlapThread = {[](std::stop_token stop) {
    // We're racing the tablet driver; give it some time to catch up
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (stop.stop_requested()) {
      return;
    }
    if (gInstance->p->mCtx) {
      gInstance->p->mWintab.WTOverlap(gInstance->p->mCtx, true);
    }
  }};
}

WintabTablet::Impl::Impl(HWND window) {
  if (!mWintab) {
    return;
  }

  constexpr std::wstring_view contextName {L"OpenKneeboard"};

  LOGCONTEXTW logicalContext {};
  mWintab.WTInfoW(WTI_DEFCONTEXT, 0, &logicalContext);
  wcsncpy_s(
    static_cast<wchar_t*>(logicalContext.lcName),
    std::size(logicalContext.lcName),
    contextName.data(),
    contextName.size());
  logicalContext.lcPktData = PACKETDATA;
  logicalContext.lcMoveMask = PACKETDATA;
  logicalContext.lcPktMode = PACKETMODE;
  logicalContext.lcOptions |= CXO_MESSAGES;
  logicalContext.lcOptions &= ~static_cast<UINT>(CXO_SYSTEM);
  logicalContext.lcBtnDnMask = ~0;
  logicalContext.lcBtnUpMask = ~0;
  logicalContext.lcSysMode = false;

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

  for (UINT i = 0, tag; mWintab.WTInfoW(WTI_EXTENSIONS + i, EXT_TAG, &tag);
       ++i) {
    if (tag == WTX_EXPKEYS2 || tag == WTX_OBT) {
      UINT mask = 0;
      mWintab.WTInfoW(WTI_EXTENSIONS + i, EXT_MASK, &mask);
      logicalContext.lcPktData |= mask;
      break;
    }
  }

  mCtx = mWintab.WTOpenW(window, &logicalContext, true);
  if (!mCtx) {
    dprint("Failed to open wintab tablet");
    return;
  }
  dprint("Opened wintab tablet");
  mEventHook.reset(SetWinEventHook(
    EVENT_SYSTEM_FOREGROUND,
    EVENT_SYSTEM_FOREGROUND,
    NULL,
    &WinEventProc_SetOverlap,
    NULL,
    NULL,
    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
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
    if (packet.pkChanged & PK_X) {
      p->mState.x = packet.pkX;
    }
    if (packet.pkChanged & PK_Y) {
      p->mState.y = packet.pkY;
    }
    if (packet.pkChanged & PK_NORMAL_PRESSURE) {
      p->mState.pressure = packet.pkNormalPressure;
    }
    if (packet.pkChanged & PK_BUTTONS) {
      p->mState.penButtons = static_cast<uint16_t>(packet.pkButtons);
    }
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
