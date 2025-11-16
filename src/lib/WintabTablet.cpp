// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/WintabTablet.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <stdexcept>
#include <thread>

// These macros are part of the WinTab API; best of 1970 :)
// NOLINTBEGIN(cppcoreguidelines-macro-to-enum)
// clang-format off
#include <wintab/WINTAB.H>
#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CHANGED)
#define PACKETMODE 0
#define PACKETEXPKEYS PKEXT_ABSOLUTE
#include <wintab/PKTDEF.H>
// clang-format on
// NOLINTEND(cppcoreguidelines-macro-to-enum)

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
  ~LibWintab() = default;

  operator bool() const {
    return mWintab != 0;
  }

  LibWintab(const LibWintab&) = delete;
  LibWintab(LibWintab&&) = delete;
  LibWintab& operator=(const LibWintab&) = delete;
  LibWintab& operator=(LibWintab&&) = delete;

 private:
  unique_hmodule mWintab = 0;
};
}// namespace

static WintabTablet* gInstance {nullptr};

struct WintabTablet::Impl {
  Impl(HWND window, Priority priority);
  ~Impl();

  Priority mPriority;
  TabletState mState {};
  TabletInfo mDeviceInfo {};
  LibWintab mWintab {};
  HCTX mCtx {nullptr};

  unique_hwineventhook mEventHook;

  Impl() = delete;
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;
};

WintabTablet::WintabTablet(HWND window, Priority priority)
  : p(std::make_unique<Impl>(window, priority)) {
  if (gInstance) {
    throw std::runtime_error("Only one WintabTablet at a time!");
  }
  gInstance = this;
}

WintabTablet::~WintabTablet() {
  gInstance = nullptr;
}

TabletState WintabTablet::GetState() const {
  if (!p) {
    return {};
  }
  return p->mState;
}

TabletInfo WintabTablet::GetDeviceInfo() const {
  if (!p) {
    return {};
  }
  return p->mDeviceInfo;
};

bool WintabTablet::IsValid() const {
  return p && p->mCtx;
}

WintabTablet::Impl::Impl(HWND window, Priority priority) : mPriority(priority) {
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

  for (UINT i = 0, tag = 0; mWintab.WTInfoW(WTI_EXTENSIONS + i, EXT_TAG, &tag);
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

  mDeviceInfo = {
    .mMaxX = static_cast<float>(logicalContext.lcOutExtX),
    .mMaxY = static_cast<float>(-logicalContext.lcOutExtY),
    .mMaxPressure = static_cast<uint32_t>(axis.axMax),
  };

  // Populate name
  {
    std::wstring buf;
    buf.resize(
      mWintab.WTInfoW(WTI_DEVICES, DVC_NAME, nullptr) / sizeof(wchar_t));
    const auto actualSize = mWintab.WTInfoW(WTI_DEVICES, DVC_NAME, buf.data());
    buf.resize((actualSize / sizeof(wchar_t)) - 1);
    mDeviceInfo.mDeviceName = to_utf8(buf);
  }

  // Populate ID
  {
    std::wstring buf;
    buf.resize(
      mWintab.WTInfoW(WTI_DEVICES, DVC_PNPID, nullptr) / sizeof(wchar_t));
    const auto actualSize = mWintab.WTInfoW(WTI_DEVICES, DVC_PNPID, buf.data());
    buf.resize((actualSize / sizeof(wchar_t)) - 1);
    mDeviceInfo.mDeviceID = to_utf8(buf);
  }
}

WintabTablet::Impl::~Impl() {
  if (!mCtx) {
    return;
  }
  mWintab.WTClose(mCtx);
}

bool WintabTablet::CanProcessMessage(UINT message) const {
  return message == WT_PROXIMITY || message == WT_PACKET
    || message == WT_PACKETEXT || message == WT_CTXOVERLAP;
}

bool WintabTablet::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WT_PROXIMITY) {
    // high word indicates hardware events, low word indicates
    // context enter/leave
    p->mState.mIsActive = lParam & 0xffff;
    return true;
  }

  if (message == WT_CTXOVERLAP && p->mPriority == Priority::AlwaysActive) {
    if (
      reinterpret_cast<HCTX>(wParam) == gInstance->p->mCtx
      && !(static_cast<UINT>(lParam) & CXS_ONTOP)) {
      gInstance->p->mWintab.WTOverlap(gInstance->p->mCtx, TRUE);
    }
  };

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
      p->mState.mX = static_cast<float>(packet.pkX);
    }
    if (packet.pkChanged & PK_Y) {
      p->mState.mY = static_cast<float>(packet.pkY);
    }
    if (packet.pkChanged & PK_NORMAL_PRESSURE) {
      p->mState.mPressure = packet.pkNormalPressure;
    }
    if (packet.pkChanged & PK_BUTTONS) {
      p->mState.mPenButtons = static_cast<uint32_t>(packet.pkButtons);
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
      p->mState.mAuxButtons |= mask;
    } else {
      p->mState.mAuxButtons &= ~mask;
    }
    return true;
  }

  return false;
}

WintabTablet::Priority WintabTablet::GetPriority() const {
  return p->mPriority;
}

void WintabTablet::SetPriority(Priority priority) {
  p->mPriority = priority;
}

}// namespace OpenKneeboard
