// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <Windows.h>

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

}// namespace

struct Detail::FrameMetadata final {
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
  Config mConfig;

  uint8_t mLayerCount = 0;
  LayerConfig mLayers[MaxViewCount];

  DWORD mFeederProcessID {};
  // If you're looking for texture size, it's in Config
  FixedSizeHandle mTexture {};
  FixedSizeHandle mFence {};

  alignas(2 * sizeof(LONG64))
    std::array<LONG64, SHMSwapchainLength> mFrameReadyFenceValues {0};

  uint64_t GetRenderCacheKey() const;
  bool HaveFeeder() const;
};
using Detail::FrameMetadata;
static_assert(std::is_standard_layout_v<FrameMetadata>);
static constexpr DWORD SHM_SIZE = sizeof(FrameMetadata);

struct Detail::IPCHandles {
 public:
  winrt::handle mTextureHandle;
  winrt::handle mFenceHandle;

  const HANDLE mForeignTextureHandle {};
  const HANDLE mForeignFenceHandle {};

  IPCHandles(HANDLE feederProcess, const FrameMetadata& frame)
    : mForeignTextureHandle(frame.mTexture), mForeignFenceHandle(frame.mFence) {
    const auto thisProcess = GetCurrentProcess();
    winrt::check_bool(DuplicateHandle(
      feederProcess,
      frame.mFence,
      thisProcess,
      mFenceHandle.put(),
      NULL,
      FALSE,
      DUPLICATE_SAME_ACCESS));
    winrt::check_bool(DuplicateHandle(
      feederProcess,
      frame.mTexture,
      thisProcess,
      mTextureHandle.put(),
      NULL,
      FALSE,
      DUPLICATE_SAME_ACCESS));
  }

  ~IPCHandles() = default;

  IPCHandles() = delete;
  IPCHandles(const IPCHandles&) = delete;
  IPCHandles(IPCHandles&&) = delete;
  IPCHandles& operator=(const IPCHandles&) = delete;
  IPCHandles& operator=(IPCHandles&&) = delete;
};
using Detail::IPCHandles;

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

Snapshot::Snapshot(nullptr_t) : mState(State::Empty) {
}

Snapshot::Snapshot(incorrect_gpu_t) : mState(State::IncorrectGPU) {
}

Snapshot::Snapshot(ipc_handle_error_t) : mState(State::IPCHandleError) {
}

Snapshot::Snapshot(FrameMetadata* metadata) : mState(State::Empty) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Snapshot::Snapshot(FrameMetadata)");
  mHeader = std::make_shared<FrameMetadata>(*metadata);

  if (mHeader && mHeader->HaveFeeder()) {
    mState = State::ValidWithoutTexture;
  }
}

uint64_t Snapshot::GetSessionID() const {
  if (!mHeader) {
    return {};
  }

  return mHeader->mSessionID;
}

Snapshot::Snapshot(
  FrameMetadata* metadata,
  IPCTextureCopier* copier,
  IPCHandles* source,
  const std::shared_ptr<IPCClientTexture>& dest)
  : mIPCTexture(dest), mState(State::Empty) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::Snapshot::Snapshot(metadataAndTextures)");

  const auto textureIndex = metadata->mFrameNumber % SHMSwapchainLength;
  auto fenceValue = &metadata->mFrameReadyFenceValues.at(textureIndex);
  const auto fenceIn = *fenceValue;

  mHeader = std::make_shared<FrameMetadata>(*metadata);

  {
    OPENKNEEBOARD_TraceLoggingScope("CopyTexture");
    copier->Copy(
      source->mTextureHandle.get(),
      dest.get(),
      source->mFenceHandle.get(),
      fenceIn);
  }

  if (mHeader && mHeader->HaveFeeder()) {
    TraceLoggingWriteTagged(activity, "MarkingValid");
    if (mHeader->mLayerCount > 0) {
      mState = State::ValidWithTexture;
    } else {
      mState = State::ValidWithoutTexture;
    }
  }
}

Snapshot::State Snapshot::GetState() const {
  return mState;
}

Snapshot::~Snapshot() {
}

uint64_t Snapshot::GetRenderCacheKey() const {
  // This is lazy, and only works because:
  // - session ID already contains random data
  // - we're only combining *one* other value which isn't
  // If adding more data, it either needs to be random,
  // or need something like boost::hash_combine()
  return mHeader->GetRenderCacheKey();
}

uint64_t Snapshot::GetSequenceNumberForDebuggingOnly() const {
  if (!this->HasMetadata()) {
    return 0;
  }
  return mHeader->mFrameNumber;
}

Config Snapshot::GetConfig() const {
  if (!this->HasMetadata()) {
    return {};
  }
  return mHeader->mConfig;
}

uint8_t Snapshot::GetLayerCount() const {
  if (!this->HasMetadata()) {
    return 0;
  }
  return mHeader->mLayerCount;
}

const LayerConfig* Snapshot::GetLayerConfig(uint8_t layerIndex) const {
  if (layerIndex >= this->GetLayerCount()) [[unlikely]] {
    fatal(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
  }

  return &mHeader->mLayers[layerIndex];
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
  FrameMetadata* mHeader = nullptr;

  Impl() {
    // For debugging 32-bit/64-bit interop stuff
    static std::once_flag sDumpLayoutOnce;
    std::call_once(sDumpLayoutOnce, []() {
      dprint(
        "SHM information:\n"
        L"- Path: {}\n"
        L"- Size: {}\n"
        L"- Config offset: {}\n"
        L"- Config size: {}\n"
        L"- Layer config offset: {}\n"
        L"- Layer config size: {}\n"
        L"- Feeder PID offset: {}\n"
        L"- Feeder PID size: {}\n",
        SHMPath(),
        SHM_SIZE,
        offsetof(FrameMetadata, mConfig),
        sizeof(FrameMetadata::mConfig),
        offsetof(FrameMetadata, mLayers),
        sizeof(LayerConfig),
        offsetof(FrameMetadata, mFeederProcessID),
        sizeof(FrameMetadata::mFeederProcessID));
    });

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
    mHeader = reinterpret_cast<FrameMetadata*>(mMapping);
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

  *p->mHeader = FrameMetadata {};
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
  p->mHeader->mFrameNumber++;
  p->mHeader->mLayerCount = 0;
}

Writer::NextFrameInfo Writer::BeginFrame() noexcept {
  using State = WriterState;
  p->Transition<State::Locked, State::FrameInProgress>();

  const auto textureIndex
    = static_cast<uint8_t>((p->mHeader->mFrameNumber + 1) % SHMSwapchainLength);
  auto fenceValue = &p->mHeader->mFrameReadyFenceValues[textureIndex];
  const auto fenceOut = InterlockedIncrement64(fenceValue);

  return NextFrameInfo {
    .mTextureIndex = textureIndex,
    .mFenceOut = fenceOut,
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
  winrt::handle mFeederProcessHandle;
  uint64_t mSessionID {~(0ui64)};

  std::array<std::unique_ptr<IPCHandles>, SHMSwapchainLength> mHandles;

  void UpdateSession() {
    OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::Impl::UpdateSession()");
    const auto& metadata = *this->mHeader;

    if (mSessionID != metadata.mSessionID) {
      mFeederProcessHandle = {};
      mHandles = {};
      mSessionID = metadata.mSessionID;
    }

    if (mFeederProcessHandle) {
      return;
    }

    mFeederProcessHandle = winrt::handle {
      OpenProcess(PROCESS_DUP_HANDLE, FALSE, metadata.mFeederProcessID)};
  }

 private:
  // Only valid in the feeder process, but keep track of them to see if they
  // change
  HANDLE mForeignTextureHandle {};
  HANDLE mForeignFenceHandle {};
};

uint64_t Reader::GetSessionID() const {
  if (!p) {
    return {};
  }
  if (!p->mHeader) {
    return {};
  }
  return p->mHeader->mSessionID;
}

Reader::Reader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::Reader()");
  const auto path = SHMPath();
  dprint(L"Initializing SHM reader");

  this->p = std::make_shared<Impl>();
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

CachedReader::CachedReader(IPCTextureCopier* copier, ConsumerKind kind)
  : mTextureCopier(copier), mConsumerKind(kind) {
}

void CachedReader::InitializeCache(uint64_t gpuLUID, uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "SwapchainLength"));
  mGPULUID = gpuLUID;
  mCache = {};
  mCacheKey = {};
  mClientTextures = {swapchainLength, nullptr};
}

CachedReader::~CachedReader() = default;

Snapshot Reader::MaybeGetUncached(ConsumerKind kind) {
  return MaybeGetUncached({}, nullptr, nullptr, kind);
}

Snapshot Reader::MaybeGetUncached(
  uint64_t gpuLUID,
  IPCTextureCopier* copier,
  const std::shared_ptr<IPCClientTexture>& dest,
  ConsumerKind kind) const {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::Reader::MaybeGetUncached()");
  const auto transitions = make_scoped_state_transitions<
    State::Locked,
    State::CreatingSnapshot,
    State::Locked>(p);

  p->UpdateSession();

  if (!(gpuLUID && copier && dest)) {
    return Snapshot(p->mHeader);
  }

  if (p->mHeader->mGPULUID != gpuLUID) {
    TraceLoggingWriteTagged(
      activity,
      "SHM::Reader::MaybeGetUncached/incorrect_gpu",
      TraceLoggingValue(p->mHeader->mGPULUID, "FeederLUID"),
      TraceLoggingValue(gpuLUID, "ReaderLUID"));
    activity.StopWithResult("incorrect_gpu");
    return {Snapshot::incorrect_gpu};
  }

  auto& handles = p->mHandles.at(p->mHeader->mFrameNumber % SHMSwapchainLength);
  if (handles && (
    (handles->mForeignFenceHandle != p->mHeader->mFence) 
  ||
    (handles->mForeignTextureHandle != p->mHeader->mTexture) )) {
    // Impl::UpdateSession() should have nuked the whole lot
    dprint("Replacing handles without new session ID");
    OPENKNEEBOARD_BREAK;
    handles = {};
  }
  if (!handles) {
    try {
      handles = std::make_unique<IPCHandles>(
        p->mFeederProcessHandle.get(), *p->mHeader);
    } catch (const winrt::hresult_error& e) {
      TraceLoggingWriteTagged(
        activity,
        "SHM::Reader::MaybeGetUncached/ipc_handle_error",
        TraceLoggingValue(e.code().value, "HRESULT"),
        TraceLoggingValue(e.message().c_str(), "Message"));
      activity.StopWithResult("ipc_handle_error");
      return {Snapshot::ipc_handle_error};
    }
  }

  return Snapshot(p->mHeader, copier, handles.get(), dest);
}

uint64_t Reader::GetRenderCacheKey(ConsumerKind kind) const {
  if (!(p && p->mHeader)) {
    return {};
  }

  ActiveConsumers::Set(kind);

  return p->mHeader->GetRenderCacheKey();
}

void Writer::SubmitFrame(
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
  p->mHeader->mConfig = config;
  p->mHeader->mFrameNumber++;
  p->mHeader->mFlags |= HeaderFlags::FEEDER_ATTACHED;
  p->mHeader->mLayerCount = static_cast<uint8_t>(layers.size());
  p->mHeader->mFeederProcessID = p->mProcessID;
  p->mHeader->mTexture = texture;
  p->mHeader->mFence = fence;
  memcpy(
    p->mHeader->mLayers, layers.data(), sizeof(LayerConfig) * layers.size());
}

bool FrameMetadata::HaveFeeder() const {
  return (mMagic == *reinterpret_cast<const uint64_t*>(Magic.data()))
    && ((mFlags & HeaderFlags::FEEDER_ATTACHED)
        == HeaderFlags::FEEDER_ATTACHED);
}

uint64_t FrameMetadata::GetRenderCacheKey() const {
  // This is lazy, and only works because:
  // - session ID already contains random data
  // - we're only combining *one* other value which isn't
  // If adding more data, it either needs to be random,
  // or need something like boost::hash_combine()
  std::hash<uint64_t> HashUI64;
  return HashUI64(mSessionID) ^ HashUI64(mFrameNumber);
}

uint64_t Reader::GetFrameCountForMetricsOnly() const {
  if (!(p && p->mHeader)) {
    return {};
  }
  return p->mHeader->mFrameNumber;
}

ConsumerPattern::ConsumerPattern() = default;
ConsumerPattern::ConsumerPattern(
  std::underlying_type_t<ConsumerKind> consumerKindMask)
  : mKindMask(consumerKindMask) {
}

std::underlying_type_t<ConsumerKind> ConsumerPattern::GetRawMaskForDebugging()
  const {
  return mKindMask;
}

bool ConsumerPattern::Matches(ConsumerKind kind) const {
  return (mKindMask & std::to_underlying(kind)) == mKindMask;
}

Snapshot CachedReader::MaybeGet(const std::source_location& loc) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "CachedReader::MaybeGet()");
  if (!(*this)) {
    TraceLoggingWriteStop(
      activity,
      "CachedReader::MaybeGet()",
      TraceLoggingValue("Invalid SHM", "Result"));
    return {nullptr};
  }

  this->UpdateSession();

  ActiveConsumers::Set(mConsumerKind);

  const auto swapchainIndex = mSwapchainIndex;
  if (swapchainIndex >= mClientTextures.size()) [[unlikely]] {
    fatal(
      loc,
      "swapchainIndex {} >= swapchainLength {}; did you call "
      "InitializeCache()?",
      swapchainIndex,
      mClientTextures.size());
  }
  mSwapchainIndex = (mSwapchainIndex + 1) % mClientTextures.size();

  const auto cacheKey = this->GetRenderCacheKey(mConsumerKind);

  if (cacheKey == mCacheKey) {
    const auto& cache = mCache.front();
    if (cache.GetState() == Snapshot::State::ValidWithTexture) {
      TraceLoggingWriteStop(
        activity,
        "CachedReader::MaybeGet()",
        TraceLoggingValue("Returning cached snapshot", "Result"),
        TraceLoggingValue(
          static_cast<unsigned int>(cache.GetState()), "State"));
      return cache;
    }
  }

  TraceLoggingWriteTagged(activity, "LockingSHM");
  std::unique_lock lock(*p);
  TraceLoggingWriteTagged(activity, "LockedSHM");
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    maybeGetActivity, "MaybeGetUncached");

  if (p->mHeader->mLayerCount == 0) {
    maybeGetActivity.StopWithResult("NoLayers");
    return Snapshot {p->mHeader};
  }

  const auto dimensions = p->mHeader->mConfig.mTextureSize;
  auto dest = GetIPCClientTexture(dimensions, swapchainIndex);

  auto snapshot
    = this->MaybeGetUncached(mGPULUID, mTextureCopier, dest, mConsumerKind);
  const auto state = snapshot.GetState();
  maybeGetActivity.StopWithResult(static_cast<int>(state));

  if (state == Snapshot::State::Empty) {
    const auto& cache = mCache.front();
    TraceLoggingWriteStop(
      activity,
      "CachedReader::MaybeGet()",
      TraceLoggingValue("Using stale cache", "Result"),
      TraceLoggingValue(static_cast<unsigned int>(cache.GetState()), "State"));
    return cache;
  }

  mCache.push_front(snapshot);
  mCache.resize(mClientTextures.size(), nullptr);
  mCacheKey = cacheKey;

  TraceLoggingWriteStop(
    activity,
    "CachedReader::MaybeGet()",
    TraceLoggingValue("Updated cache", "Result"),
    TraceLoggingValue(static_cast<unsigned int>(state), "State"),
    TraceLoggingValue(
      snapshot.GetSequenceNumberForDebuggingOnly(), "SequenceNumber"));
  return snapshot;
}

std::shared_ptr<IPCClientTexture> CachedReader::GetIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "CachedReader::GetIPCClientTexture",
    TraceLoggingValue(swapchainIndex, "swapchainIndex"));
  auto& ret = mClientTextures.at(swapchainIndex);
  if (ret && ret->GetDimensions() != dimensions) {
    ret = {};
  }

  if (!ret) {
    OPENKNEEBOARD_TraceLoggingScope("CachedReader::CreateIPCClientTexture");
    ret = this->CreateIPCClientTexture(dimensions, swapchainIndex);
  }
  return ret;
}

Snapshot CachedReader::MaybeGetMetadata() {
  OPENKNEEBOARD_TraceLoggingScope("CachedReader::MaybeGetMetadata()");

  if (!*this) {
    return {nullptr};
  }

  this->UpdateSession();

  const auto cacheKey = this->GetRenderCacheKey(mConsumerKind);

  if ((!mCache.empty()) && cacheKey == mCacheKey) {
    return mCache.front();
  }

  if (!mClientTextures.empty()) {
    return MaybeGet();
  }

  std::unique_lock lock(*p);
  auto snapshot = this->MaybeGetUncached(mConsumerKind);
  if (snapshot.HasMetadata()) {
    mCache.push_front(snapshot);
    mCacheKey = cacheKey;
  }

  return snapshot;
}

void CachedReader::UpdateSession() {
  const auto sessionID = this->GetSessionID();
  if (sessionID == mSessionID) {
    return;
  }
  this->ReleaseIPCHandles();
  p->UpdateSession();
  mSessionID = sessionID;
}

IPCClientTexture::IPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex)
  : mDimensions(dimensions), mSwapchainIndex(swapchainIndex) {
}

IPCClientTexture::~IPCClientTexture() = default;
IPCTextureCopier::~IPCTextureCopier() = default;

}// namespace OpenKneeboard::SHM
