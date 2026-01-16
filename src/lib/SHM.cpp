// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "OpenKneeboard/numeric_cast.hpp"
#include "SHM/ReaderState.hpp"
#include "SHM/WriterState.hpp"

#include <OpenKneeboard/LazyOnceValue.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/StateMachine.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/bitflags.hpp>
#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <Windows.h>

#include <wil/resource.h>

#include <bit>
#include <concepts>
#include <format>
#include <random>
#include <utility>

#include <processthreadsapi.h>

namespace OpenKneeboard::SHM {

enum class HeaderFlags : ULONG {
  FEEDER_ATTACHED = 1 << 0,
};

namespace {
using namespace Detail;
}

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<SHM::HeaderFlags> = true;
}// namespace OpenKneeboard

namespace OpenKneeboard::SHM {
static uint64_t CreateSessionID() {
  std::random_device randDevice;
  std::uniform_int_distribution<uint32_t> randDist;
  return (static_cast<uint64_t>(GetCurrentProcessId()) << 32)
    | randDist(randDevice);
}

namespace {
/* Wrapper for a HANDLE that is always 64-bits large
 *
 * From
 * https://learn.microsoft.com/en-us/windows/win32/winprog64/interprocess-communication:
 *
 * > 64-bit versions of Windows use 32-bit handles for interoperability. When
 * > sharing a handle between 32-bit and 64-bit applications, only the lower 32
 * > bits are significant, so it is safe to truncate the handle (when passing it
 * > from 64-bit to 32-bit) or sign-extend the handle (when passing it from
 * > 32-bit to 64-bit).
 */
class FixedSizeHandle final {
 public:
  FixedSizeHandle() noexcept = default;

  FixedSizeHandle(HANDLE value) noexcept
    : mValue(std::bit_cast<uintptr_t>(value)) {
  }

  operator HANDLE() const noexcept {
    return std::bit_cast<HANDLE>(static_cast<uintptr_t>(mValue));
  }

 private:
  uint64_t mValue {};
};
static_assert(sizeof(FixedSizeHandle) == sizeof(uint64_t));

struct SharedData final {
  struct Frame {
    FixedSizeHandle mTexture {nullptr};
    FixedSizeHandle mFence {nullptr};
    int64_t mReadyReadFenceValue {};

    Config mConfig {};
    LayerConfig mLayers[MaxViewCount];

    uint32_t mLayerCount {};
  };

  // Use the magic string to make sure we don't have
  // uninitialized memory that happens to have the
  // feeder-attached bit set
  static constexpr std::string_view Magic {"OKBMagic"};
  static_assert(Magic.size() == sizeof(uint64_t));
  uint64_t mMagic = *reinterpret_cast<const uint64_t*>(Magic.data());

  uint64_t mGPULUID {};

  uint64_t mFrameNumber = 0;
  uint64_t mSessionID = CreateSessionID();
  HeaderFlags mFlags;

  DWORD mFeederProcessID {};

  uint8_t mPadding[8];
  Frame mFrames[SwapChainLength];

  [[nodiscard]]
  bool HaveFeeder() const;
};

static_assert(std::is_standard_layout_v<SharedData>);
constexpr DWORD SHM_SIZE = sizeof(SharedData);
static_assert(
  SHM_SIZE == 3024,
  "Potential mismatch between 32-bit and 64-bit SHM layout");

}// namespace

static std::wstring SHMPath() {
  static auto sRet = LazyOnceValue<std::wstring> {[] {
    return std::format(
      L"{}/{}.{}.{}.{}-s{:x}",
      ProjectReverseDomainW,
      Version::Major,
      Version::Minor,
      Version::Patch,
      Version::Build,
      SHM_SIZE);
  }};
  return sRet;
}

static std::wstring MutexPath() {
  static auto sRet
    = LazyOnceValue<std::wstring> {[] { return SHMPath() + L".mutex"; }};
  return sRet;
}

template <class T>
concept shm_state_machine = lockable_state_machine<T> && (T::HasFinalState)
  && (T::FinalState == T::Values::Unlocked);

template <shm_state_machine TStateMachine>
class Impl {
 public:
  using State = TStateMachine::Values;
  winrt::handle mFileHandle;
  winrt::handle mMutexHandle;
  std::byte* mMapping = nullptr;
  SharedData* mHeader = nullptr;

  Impl() {
    auto fileHandle = Win32::or_default::CreateFileMapping(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      0,
      DWORD {SHM_SIZE},// Perfect forwarding fails with static constexpr
                       // integer values
      SHMPath().c_str());
    if (!fileHandle) {
      dprint("CreateFileMapping failed: {}", static_cast<int>(GetLastError()));
      return;
    }

    auto mutexHandle
      = Win32::or_default::CreateMutex(nullptr, FALSE, MutexPath().c_str());
    if (!mutexHandle) {
      dprint("CreateMutexW failed: {}", static_cast<int>(GetLastError()));
      return;
    }

    mMapping = reinterpret_cast<std::byte*>(
      MapViewOfFile(fileHandle.get(), FILE_MAP_WRITE, 0, 0, SHM_SIZE));
    if (!mMapping) {
      dprint(
        "MapViewOfFile failed: {:#x}", std::bit_cast<uint32_t>(GetLastError()));
      return;
    }

    mFileHandle = std::move(fileHandle);
    mMutexHandle = std::move(mutexHandle);
    mHeader = reinterpret_cast<SharedData*>(mMapping);
  }

  ~Impl() {
    UnmapViewOfFile(mMapping);
  }

  bool IsValid() const {
    return mMapping;
  }

  template <State in, State out>
  void Transition(
    const std::source_location& loc = std::source_location::current()) {
    mState.template Transition<in, out>(loc);
  }

  // "Lockable" C++ named concept: supports std::unique_lock

  void lock() {
    mState.template Transition<State::Unlocked, State::TryLock>();
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::lock()");

    const auto result = WaitForSingleObject(mMutexHandle.get(), INFINITE);
    switch (result) {
      case WAIT_OBJECT_0:
        // success
        break;
      case WAIT_ABANDONED:
        *mHeader = {};
        break;
      default:
        mState.template Transition<State::TryLock, State::Unlocked>();
        TraceLoggingWriteStop(
          activity, "SHM::Impl::lock()", TraceLoggingValue(result, "Error"));
        dprint(
          "Unexpected result from SHM WaitForSingleObject in lock(): "
          "{:#016x}",
          static_cast<uint64_t>(result));
        OPENKNEEBOARD_BREAK;
        return;
    }

    mState.template Transition<State::TryLock, State::Locked>();
    TraceLoggingWriteStop(
      activity, "SHM::Impl::lock()", TraceLoggingValue("Success", "Result"));
  }

  bool try_lock() {
    mState.template Transition<State::Unlocked, State::TryLock>();
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::try_lock()");

    const auto result = WaitForSingleObject(mMutexHandle.get(), 0);
    switch (result) {
      case WAIT_OBJECT_0:
        // success
        break;
      case WAIT_ABANDONED:
        *mHeader = {};
        break;
      case WAIT_TIMEOUT:
        // expected in try_lock()
        mState.template Transition<State::TryLock, State::Unlocked>();
        return false;
      default:
        mState.template Transition<State::TryLock, State::Unlocked>();
        TraceLoggingWriteStop(
          activity,
          "SHM::Impl::try_lock()",
          TraceLoggingValue(result, "Error"));
        dprint(
          "Unexpected result from SHM WaitForSingleObject in try_lock(): {}",
          result);
        OPENKNEEBOARD_BREAK;
        return false;
    }

    mState.template Transition<State::TryLock, State::Locked>();
    TraceLoggingWriteStop(
      activity,
      "SHM::Impl::try_lock()",
      TraceLoggingValue("Success", "Result"));
    return true;
  }

  void unlock() {
    mState.template Transition<State::Locked, State::Unlocked>();
    OPENKNEEBOARD_TraceLoggingScope("SHM::Impl::unlock()");
    ReleaseMutex(mMutexHandle.get());
  }

  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

 protected:
  TStateMachine mState;
};

class Writer::Impl : public SHM::Impl<WriterStateMachine> {
 public:
  using SHM::Impl<WriterStateMachine>::Impl;

  DWORD mProcessID = GetCurrentProcessId();
  uint64_t mGPULUID {};
  int64_t mReadyReadFenceValue {};
};

Writer::Writer(uint64_t gpuLUID) {
  const auto path = SHMPath();
  dprint(L"Initializing SHM writer");

  p = std::make_shared<Impl>();
  if (!p->IsValid()) {
    p.reset();
    return;
  }
  p->mGPULUID = gpuLUID;

  *p->mHeader = SharedData {};
  dprint("Writer initialized.");
}

void Writer::Detach() {
  p->Transition<State::Locked, State::Detaching>();

  const auto oldID = p->mHeader->mSessionID;
  *p->mHeader = {};
  FlushViewOfFile(p->mMapping, NULL);

  p->Transition<State::Detaching, State::Locked>();
  dprint(
    "Writer::Detach(): Session ID {:#018x} replaced with {:#018x}",
    oldID,
    p->mHeader->mSessionID);
}

Writer::~Writer() {
  std::unique_lock lock(*this);
  this->Detach();
}

void Writer::SubmitEmptyFrame() {
  using State = WriterState;
  const auto transitions = make_scoped_state_transitions<
    State::Locked,
    State::SubmittingEmptyFrame,
    State::Locked>(p);

  const auto idx = (++p->mHeader->mFrameNumber) % SwapChainLength;
  p->mHeader->mFrames[idx].mLayerCount = 0;
}

uint64_t Writer::GetFrameCountForMetricsOnly() const {
  return p->mHeader->mFrameNumber;
}

Writer::NextFrameInfo Writer::BeginFrame() noexcept {
  using State = WriterState;
  p->Transition<State::Locked, State::FrameInProgress>();

  const auto textureIndex
    = static_cast<uint8_t>((p->mHeader->mFrameNumber + 1) % SHMSwapchainLength);

  return NextFrameInfo {
    .mTextureIndex = textureIndex,
    .mFenceOut = ++p->mReadyReadFenceValue,
  };
}

void Writer::lock() {
  p->lock();
}

void Writer::unlock() {
  p->unlock();
}

bool Writer::try_lock() {
  return p->try_lock();
}

class Reader::Impl : public SHM::Impl<ReaderStateMachine> {
 public:
  struct IPCHandle {
    HANDLE mForeignHandle {};
    wil::unique_handle mMyHandle {};
    void Update(HANDLE sharedFrom, HANDLE sharedHandle);

    operator bool() const noexcept {
      return static_cast<bool>(mMyHandle);
    }

    operator HANDLE() const noexcept {
      return mMyHandle.get();
    }
  };
  struct FrameHandles {
    IPCHandle mTexture {};
    IPCHandle mFence {};
  };
  using FrameHandleMap = std::array<FrameHandles, SHMSwapchainLength>;
  struct SessionResources {
    wil::unique_process_handle mFeederProcess;
    FrameHandleMap mFrameHandles;
  };

  uint64_t mGpuLUID {};
  ConsumerKind mConsumerKind {};

  uint64_t mSessionID {std::numeric_limits<uint64_t>::max()};
  SessionResources mSessionResources {};

  void UpdateSession() {
    OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::Impl::UpdateSession()");
    const auto& shared = *this->mHeader;

    if (shared.mSessionID != mSessionID) {
      mSessionResources = {};
      mSessionID = shared.mSessionID;
    }
    if (!mSessionID) {
      return;
    }

    auto& [feeder, frames] = mSessionResources;
    if (!feeder) {
      feeder.reset(
        OpenProcess(PROCESS_DUP_HANDLE, FALSE, shared.mFeederProcessID));
    }
    if (!feeder) {
      OPENKNEEBOARD_BREAK;
      return;
    }
    const auto index = shared.mFrameNumber % SHMSwapchainLength;
    auto& source = shared.mFrames[index];
    auto& [texture, fence] = frames.at(numeric_cast<std::size_t>(index));
    texture.Update(feeder.get(), source.mTexture);
    fence.Update(feeder.get(), source.mFence);
  }

 private:
  // Only valid in the feeder process, but keep track of them to see if they
  // change
  HANDLE mForeignTextureHandle {};
  HANDLE mForeignFenceHandle {};
};

void Reader::Impl::IPCHandle::Update(
  const HANDLE sharedFrom,
  const HANDLE sharedHandle) {
  if (sharedHandle == mForeignHandle && mMyHandle) {
    return;
  }

  mForeignHandle = sharedHandle;

  winrt::check_bool(DuplicateHandle(
    sharedFrom,
    sharedHandle,
    GetCurrentProcess(),
    std::out_ptr(mMyHandle),
    NULL,
    FALSE,
    DUPLICATE_SAME_ACCESS));
}

Reader::Reader(const ConsumerKind kind, const uint64_t gpuLUID) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::Reader()");
  const auto path = SHMPath();
  dprint(L"Initializing SHM reader");

  p = std::make_shared<Impl>();
  p->mGpuLUID = gpuLUID;
  p->mConsumerKind = kind;

  if (!p->IsValid()) {
    p.reset();
    return;
  }
  dprint("Reader initialized.");
}

Reader::~Reader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::~Reader()");
}

Reader::operator bool() const {
  return p && p->IsValid() && p->mHeader->HaveFeeder();
}

Writer::operator bool() const {
  return (bool)p;
}

std::expected<Frame, Frame::Error> Reader::MaybeGet() {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::Reader::MaybeGetUncached()");
  const auto lock = std::unique_lock(*p);

  if (!(p->mHeader && p->mHeader->HaveFeeder())) {
    return std::unexpected {Frame::Error::NoFeeder};
  }
  const auto previousSession = p->mSessionID;
  p->UpdateSession();
  if (p->mSessionID != previousSession) {
    this->OnSessionChanged();
  }

  if (p->mHeader->mGPULUID != p->mGpuLUID) {
    TraceLoggingWriteTagged(
      activity,
      "SHM::Reader::MaybeGetUncached/incorrect_gpu",
      TraceLoggingValue(p->mHeader->mGPULUID, "FeederLUID"),
      TraceLoggingValue(p->mGpuLUID, "ReaderLUID"));
    activity.StopWithResult("incorrect_gpu");
    return std::unexpected {Frame::Error::IncorrectGPU};
  }

  ActiveConsumers::Set(p->mConsumerKind);

  const auto index = p->mHeader->mFrameNumber % SHMSwapchainLength;
  const auto& [texture, fence] = p->mSessionResources.mFrameHandles.at(
    p->mHeader->mFrameNumber % SHMSwapchainLength);
  if (!(texture && fence)) {
    return std::unexpected {Frame::Error::UnusableHandles};
  }
  const auto& frame = p->mHeader->mFrames[index];

  return Frame {
    .mConfig = frame.mConfig,
    .mLayers = {
      frame.mLayers,
      frame.mLayers + frame.mLayerCount,
    },
    .mTexture = texture,
    .mFence = fence,
    .mFenceIn = frame.mReadyReadFenceValue,
    .mIndex = static_cast<uint8_t>(index),
  };
}

void Writer::SubmitFrame(
  const NextFrameInfo& info,
  const Config& config,
  const std::vector<LayerConfig>& layers,
  HANDLE texture,
  HANDLE fence) {
  if (!p) {
    throw std::logic_error("Attempted to update invalid SHM");
  }

  const auto transitions = make_scoped_state_transitions<
    State::FrameInProgress,
    State::SubmittingFrame,
    State::Locked>(p);

  if (layers.size() > MaxViewCount) [[unlikely]] {
    fatal(
      "Asked to publish {} layers, but max is {}", layers.size(), MaxViewCount);
  }

  p->mHeader->mGPULUID = p->mGPULUID;
  p->mHeader->mFlags |= HeaderFlags::FEEDER_ATTACHED;
  p->mHeader->mFeederProcessID = p->mProcessID;
  const auto idx = (++p->mHeader->mFrameNumber) % SwapChainLength;
  OPENKNEEBOARD_ASSERT(idx == info.mTextureIndex);
  OPENKNEEBOARD_ASSERT(p->mReadyReadFenceValue == info.mFenceOut);
  auto& frame = p->mHeader->mFrames[idx];

  frame = {
    .mTexture = texture,
    .mFence = fence,
    .mReadyReadFenceValue = info.mFenceOut,
    .mConfig = config,
    .mLayerCount = static_cast<uint8_t>(layers.size()),
  };
  memcpy(frame.mLayers, layers.data(), sizeof(LayerConfig) * layers.size());
}

bool SharedData::HaveFeeder() const {
  return (mMagic == *reinterpret_cast<const uint64_t*>(Magic.data()))
    && ((mFlags & HeaderFlags::FEEDER_ATTACHED)
        == HeaderFlags::FEEDER_ATTACHED);
}

uint64_t Reader::GetFrameCountForMetricsOnly() const {
  if (!(p && p->mHeader)) {
    return {};
  }
  return p->mHeader->mFrameNumber;
}

}// namespace OpenKneeboard::SHM
