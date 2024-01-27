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
#include "SHM/ReaderState.h"
#include "SHM/WriterState.h"

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/StateMachine.h>
#include <OpenKneeboard/Win32.h>

#include <OpenKneeboard/bitflags.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <shims/utility>

#include <Windows.h>

#include <bit>
#include <concepts>
#include <format>
#include <random>

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

struct Detail::FrameMetadata final {
  // Use the magic string to make sure we don't have
  // uninitialized memory that happens to have the
  // feeder-attached bit set
  static constexpr std::string_view Magic {"OKBMagic"};
  static_assert(Magic.size() == sizeof(uint64_t));
  uint64_t mMagic = *reinterpret_cast<const uint64_t*>(Magic.data());

  uint64_t mFrameNumber = 0;
  uint64_t mSessionID = CreateSessionID();
  HeaderFlags mFlags;
  Config mConfig;

  uint8_t mLayerCount = 0;
  LayerConfig mLayers[MaxLayers];

  DWORD mFeederProcessID {};
  // If you're looking for texture size, it's in Config
  HANDLE mTexture {};
  HANDLE mFence {};
  std::array<LONG64, TextureCount> mFenceValues {0};

  size_t GetRenderCacheKey() const;
  bool HaveFeeder() const;
};
using Detail::FrameMetadata;
static_assert(std::is_standard_layout_v<FrameMetadata>);
static constexpr DWORD SHM_SIZE = sizeof(FrameMetadata);

struct Detail::IPCSwapchainBufferResources {
  winrt::handle mTextureHandle;
  winrt::handle mFenceHandle;

  IPCSwapchainBufferResources(
    HANDLE feederProcess,
    const FrameMetadata& frame) {
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

  IPCSwapchainBufferResources() = delete;
  IPCSwapchainBufferResources(const IPCSwapchainBufferResources&) = delete;
  IPCSwapchainBufferResources(IPCSwapchainBufferResources&&) = delete;
  IPCSwapchainBufferResources& operator=(const IPCSwapchainBufferResources&)
    = delete;
  IPCSwapchainBufferResources& operator=(IPCSwapchainBufferResources&&)
    = delete;
};
using Detail::IPCSwapchainBufferResources;

static auto SHMPath() {
  static std::wstring sCache;
  if (!sCache.empty()) [[likely]] {
    return sCache;
  }
  sCache = std::format(
    L"{}/{}.{}.{}.{}-s{:x}",
    ProjectNameW,
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    SHM_SIZE);
  return sCache;
}

static auto MutexPath() {
  static std::wstring sCache;
  if (sCache.empty()) [[unlikely]] {
    sCache = SHMPath() + L".mutex";
  }
  return sCache;
}

Snapshot::Snapshot(nullptr_t) : mState(State::Empty) {
}

Snapshot::Snapshot(incorrect_kind_t) : mState(State::IncorrectKind) {
}

Snapshot::Snapshot(
  FrameMetadata* metadata,
  IPCTextureCopier* copier,
  IPCSwapchainBufferResources* source,
  const std::shared_ptr<IPCClientTexture>& dest)
  : mIPCTexture(dest), mState(State::Empty) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::Snapshot::Snapshot()");

  const auto textureIndex = metadata->mFrameNumber % TextureCount;
  auto fenceValue = &metadata->mFenceValues.at(textureIndex);
  const auto fenceOut = InterlockedIncrement64(fenceValue);
  const auto fenceIn = fenceOut - 1;

  mHeader = std::make_shared<FrameMetadata>(*metadata);

  {
    OPENKNEEBOARD_TraceLoggingScope("CopyTexture");
    copier->Copy(
      source->mTextureHandle.get(),
      dest.get(),
      source->mFenceHandle.get(),
      fenceIn,
      fenceOut);
  }

  if (mHeader && mHeader->HaveFeeder() && (mHeader->mLayerCount > 0)) {
    TraceLoggingWriteTagged(activity, "MarkingValid");
    mState = State::Valid;
  }
}

Snapshot::State Snapshot::GetState() const {
  return mState;
}

Snapshot::~Snapshot() {
}

size_t Snapshot::GetRenderCacheKey() const {
  // This is lazy, and only works because:
  // - session ID already contains random data
  // - we're only combining *one* other value which isn't
  // If adding more data, it either needs to be random,
  // or need something like boost::hash_combine()
  return mHeader->GetRenderCacheKey();
}

uint64_t Snapshot::GetSequenceNumberForDebuggingOnly() const {
  if (!this->IsValid()) {
    return 0;
  }
  return mHeader->mFrameNumber;
}

Config Snapshot::GetConfig() const {
  if (!this->IsValid()) {
    return {};
  }
  return mHeader->mConfig;
}

uint8_t Snapshot::GetLayerCount() const {
  if (!this->IsValid()) {
    return 0;
  }
  return mHeader->mLayerCount;
}

const LayerConfig* Snapshot::GetLayerConfig(uint8_t layerIndex) const {
  if (layerIndex >= this->GetLayerCount()) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
  }

  return &mHeader->mLayers[layerIndex];
}

bool Snapshot::IsValid() const {
  return mState == State::Valid;
}

template <lockable_state State, State InitialState = State::Unlocked>
class Impl {
 public:
  winrt::handle mFileHandle;
  winrt::handle mMutexHandle;
  std::byte* mMapping = nullptr;
  FrameMetadata* mHeader = nullptr;

  Impl() {
    auto fileHandle = Win32::CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      0,
      DWORD {SHM_SIZE},// Perfect forwarding fails with static constexpr
                       // integer values
      SHMPath().c_str());
    if (!fileHandle) {
      dprintf(
        "CreateFileMappingW failed: {}", static_cast<int>(GetLastError()));
      return;
    }

    auto mutexHandle = Win32::CreateMutexW(nullptr, FALSE, MutexPath().c_str());
    if (!mutexHandle) {
      dprintf("CreateMutexW failed: {}", static_cast<int>(GetLastError()));
      return;
    }

    mMapping = reinterpret_cast<std::byte*>(
      MapViewOfFile(fileHandle.get(), FILE_MAP_WRITE, 0, 0, SHM_SIZE));
    if (!mMapping) {
      dprintf(
        "MapViewOfFile failed: {:#x}", std::bit_cast<uint32_t>(GetLastError()));
      return;
    }

    mFileHandle = std::move(fileHandle);
    mMutexHandle = std::move(mutexHandle);
    mHeader = reinterpret_cast<FrameMetadata*>(mMapping);
  }

  ~Impl() {
    UnmapViewOfFile(mMapping);
    if (mState.Get() != State::Unlocked) {
      using namespace OpenKneeboard::ADL;
      dprintf(
        "Closing SHM with invalid state: {}", formattable_state(mState.Get()));
      OPENKNEEBOARD_BREAK;
      std::terminate();
    }
  }

  bool IsValid() const {
    return mMapping;
  }

  template <State in, State out>
  void Transition(
    const std::source_location& loc = std::source_location::current()) {
    mState.Transition<in, out>(loc);
  }

  // "Lockable" C++ named concept: supports std::unique_lock

  void lock() {
    mState.Transition<State::Unlocked, State::TryLock>();
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
        mState.Transition<State::TryLock, State::Unlocked>();
        TraceLoggingWriteStop(
          activity, "SHM::Impl::lock()", TraceLoggingValue(result, "Error"));
        dprintf(
          "Unexpected result from SHM WaitForSingleObject in lock(): "
          "{:#016x}",
          static_cast<uint64_t>(result));
        OPENKNEEBOARD_BREAK;
        return;
    }

    mState.Transition<State::TryLock, State::Locked>();
    TraceLoggingWriteStop(
      activity, "SHM::Impl::lock()", TraceLoggingValue("Success", "Result"));
  }

  bool try_lock() {
    mState.Transition<State::Unlocked, State::TryLock>();
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
        mState.Transition<State::TryLock, State::Unlocked>();
        return false;
      default:
        mState.Transition<State::TryLock, State::Unlocked>();
        TraceLoggingWriteStop(
          activity,
          "SHM::Impl::try_lock()",
          TraceLoggingValue(result, "Error"));
        dprintf(
          "Unexpected result from SHM WaitForSingleObject in try_lock(): {}",
          result);
        OPENKNEEBOARD_BREAK;
        return false;
    }

    mState.Transition<State::TryLock, State::Locked>();
    TraceLoggingWriteStop(
      activity,
      "SHM::Impl::try_lock()",
      TraceLoggingValue("Success", "Result"));
    return true;
  }

  void unlock() {
    mState.Transition<State::Locked, State::Unlocked>();
    OPENKNEEBOARD_TraceLoggingScope("SHM::Impl::unlock()");
    const auto ret = ReleaseMutex(mMutexHandle.get());
  }

 protected:
  StateMachine<State> mState = InitialState;
};

class Writer::Impl : public SHM::Impl<WriterState> {
 public:
  using SHM::Impl<WriterState, WriterState::Unlocked>::Impl;

  DWORD mProcessID = GetCurrentProcessId();
};

Writer::Writer() {
  const auto path = SHMPath();
  dprintf(L"Initializing SHM writer with path {}", path);

  p = std::make_shared<Impl>();
  if (!p->IsValid()) {
    p.reset();
    return;
  }

  *p->mHeader = FrameMetadata {};
  dprint("Writer initialized.");
}

void Writer::Detach() {
  p->Transition<State::Locked, State::Detaching>();

  p->mHeader->mFlags &= ~HeaderFlags::FEEDER_ATTACHED;
  FlushViewOfFile(p->mMapping, NULL);

  p->Transition<State::Detaching, State::Locked>();
}

Writer::~Writer() {
  std::unique_lock lock(*this);
  this->Detach();
}

Writer::NextFrameInfo Writer::BeginFrame() noexcept {
  p->Transition<State::Locked, State::FrameInProgress>();

  const auto textureIndex
    = static_cast<uint8_t>((p->mHeader->mFrameNumber + 1) % TextureCount);
  auto fenceValue = &p->mHeader->mFenceValues[textureIndex];
  const auto fenceOut = InterlockedIncrement64(fenceValue);
  const auto fenceIn = fenceOut - 1;

  return NextFrameInfo {
    .mTextureIndex = textureIndex,
    .mFenceIn = fenceIn,
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

class Reader::Impl : public SHM::Impl<ReaderState> {
 public:
  winrt::handle mFeederProcessHandle;
  uint64_t mSessionID {~(0ui64)};

  std::array<std::unique_ptr<IPCSwapchainBufferResources>, TextureCount>
    mBufferResources;

  void UpdateSession() {
    const auto& metadata = *this->mHeader;

    if (
      mForeignTextureHandle != metadata.mTexture
      || mForeignFenceHandle != metadata.mFence) {
      mBufferResources = {};
      mForeignTextureHandle = metadata.mTexture;
      mForeignFenceHandle = metadata.mFence;
    }

    if (metadata.mSessionID == mSessionID) [[likely]] {
      return;
    }

    mFeederProcessHandle = winrt::handle {
      OpenProcess(PROCESS_DUP_HANDLE, FALSE, metadata.mFeederProcessID)};
    mSessionID = metadata.mSessionID;
    // Probably just did this, but doesn't hurt to be sure :)
    mBufferResources = {};
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
  dprintf(L"Initializing SHM reader with path {}", path);

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
  return p && p->IsValid() & p->mHeader->HaveFeeder();
}

Writer::operator bool() const {
  return (bool)p;
}

CachedReader::CachedReader(IPCTextureCopier* copier, ConsumerKind kind)
  : mTextureCopier(copier), mConsumerKind(kind) {
}

void CachedReader::InitializeCache(uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "SwapchainLength"));
  mCache.resize(swapchainLength, nullptr);
  mClientTextures = {swapchainLength, nullptr};
}

CachedReader::~CachedReader() = default;

Snapshot Reader::MaybeGetUncached(
  IPCTextureCopier* copier,
  const std::shared_ptr<IPCClientTexture>& dest,
  ConsumerKind kind) const {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Reader::MaybeGetUncached()");
  const auto transitions = make_scoped_state_transitions<
    State::Locked,
    State::CreatingSnapshot,
    State::Locked>(p);

  if (!p->mHeader->mConfig.mTarget.Matches(kind)) {
    traceprint(
      "Kind mismatch, not returning new snapshot; reader kind is {:#08x}, "
      "target kind is {:#08x}",
      static_cast<std::underlying_type_t<ConsumerKind>>(kind),
      p->mHeader->mConfig.mTarget.GetRawMaskForDebugging());
    return {Snapshot::incorrect_kind};
  }

  p->UpdateSession();

  auto& br = p->mBufferResources.at(p->mHeader->mFrameNumber % TextureCount);
  if (!br) {
    br = std::make_unique<IPCSwapchainBufferResources>(
      p->mFeederProcessHandle.get(), *p->mHeader);
  }

  return Snapshot(p->mHeader, copier, br.get(), dest);
}

size_t Reader::GetRenderCacheKey(ConsumerKind kind) {
  if (!(p && p->mHeader)) {
    return {};
  }

  if (p->mHeader->mConfig.mTarget.Matches(kind)) {
    ActiveConsumers::Set(kind);
  }

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

  if (layers.size() > MaxLayers) {
    throw std::logic_error(std::format(
      "Asked to publish {} layers, but max is {}", layers.size(), MaxLayers));
  }

  for (auto layer: layers) {
    const auto size = layer.mLocationOnTexture.mSize;

    if (size.mWidth == 0 || size.mHeight == 0) {
      throw std::logic_error("Not feeding a 0-size image");
    }
    if (size.mWidth > TextureWidth || size.mHeight > TextureHeight) {
      // logic_error as the feeder should scale/crop/whatever as needed to fit
      // in the limits
      throw std::logic_error(
        std::format("Oversized layer: {}x{}", size.mWidth, size.mHeight));
    }
  }

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

size_t FrameMetadata::GetRenderCacheKey() const {
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
  return (mKindMask & std23::to_underlying(kind)) == mKindMask;
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

  ActiveConsumers::Set(mConsumerKind);

  const auto swapchainIndex = mSwapchainIndex;
  if (swapchainIndex >= mClientTextures.size()) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
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
    TraceLoggingWriteStop(
      activity,
      "CachedReader::MaybeGet()",
      TraceLoggingValue("Returning cached snapshot", "Result"),
      TraceLoggingValue(static_cast<unsigned int>(cache.GetState()), "State"));
    return cache;
  }

  TraceLoggingWriteTagged(activity, "LockingSHM");
  std::unique_lock lock(*p);
  TraceLoggingWriteTagged(activity, "LockedSHM");
  TraceLoggingThreadActivity<gTraceProvider> maybeGetActivity;
  TraceLoggingWriteStart(maybeGetActivity, "MaybeGetUncached");

  const auto dimensions = p->mHeader->mConfig.mTextureSize;
  auto dest = GetIPCClientTexture(dimensions, swapchainIndex);

  auto snapshot = this->MaybeGetUncached(mTextureCopier, dest, mConsumerKind);
  const auto state = snapshot.GetState();
  TraceLoggingWriteStop(
    maybeGetActivity,
    "MaybeGetUncached",
    TraceLoggingValue(static_cast<unsigned int>(state), "State"));

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
    TraceLoggingValue(static_cast<unsigned int>(state), "State"));
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

IPCClientTexture::IPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex)
  : mDimensions(dimensions), mSwapchainIndex(swapchainIndex) {
}

IPCClientTexture::~IPCClientTexture() = default;
IPCTextureCopier::~IPCTextureCopier() = default;

}// namespace OpenKneeboard::SHM
