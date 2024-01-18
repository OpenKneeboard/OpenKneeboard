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
#include <OpenKneeboard/Spriting.h>
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

#include <d3d11_2.h>
#include <d3d11_3.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <processthreadsapi.h>

namespace OpenKneeboard::SHM {

static uint64_t CreateSessionID() {
  std::random_device randDevice;
  std::uniform_int_distribution<uint32_t> randDist;
  return (static_cast<uint64_t>(GetCurrentProcessId()) << 32)
    | randDist(randDevice);
}

enum class HeaderFlags : ULONG {
  FEEDER_ATTACHED = 1 << 0,
};

struct FrameMetadata final {
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

  DWORD mFeederProcessID {};
  HANDLE mFence {};
  LONG64 mFenceValue {};

  HANDLE mTexture {};

  uint8_t mLayerCount = 0;
  LayerConfig mLayers[MaxLayers];

  size_t GetRenderCacheKey() const;
  bool HaveFeeder() const;
};
static_assert(std::is_standard_layout_v<FrameMetadata>);

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<SHM::HeaderFlags> = true;
}// namespace OpenKneeboard

namespace OpenKneeboard::SHM {

static constexpr DWORD MAX_IMAGE_PX(1024 * 1024 * 8);
static constexpr DWORD SHM_SIZE = sizeof(FrameMetadata);

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

struct IPCLayerTextureReadResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;

  bool Populate(
    ID3D11DeviceContext* ctx,
    uint64_t sessionID,
    uint8_t layerIndex,
    uint32_t sequenceNumber);
};

struct IPCTextureReadResources {
  uint64_t mSessionID = 0;

  std::array<IPCLayerTextureReadResources, MaxLayers> mLayers;

  bool Populate(
    ID3D11DeviceContext* ctx,
    uint64_t sessionID,
    uint32_t sequenceNumber);
};

bool IPCTextureReadResources::Populate(
  ID3D11DeviceContext* ctx,
  uint64_t sessionID,
  uint32_t sequenceNumber) {
  if (sessionID != mSessionID) {
    dprintf(
      "Replacing OpenKneeboard TextureReadResources: {:0x}/{}",
      sessionID,
      sequenceNumber % TextureCount);
    *this = {.mSessionID = sessionID};
  }

  for (uint8_t i = 0; i < MaxLayers; ++i) {
    if (!mLayers[i].Populate(ctx, sessionID, i, sequenceNumber)) {
      *this = {};
      return false;
    }
  }

  return true;
}

bool IPCLayerTextureReadResources::Populate(
  ID3D11DeviceContext* ctx,
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber) {
  return false;
  /* FIXME
 if (mTexture) {
   return true;
 }

 winrt::com_ptr<ID3D11Device> device;
 ctx->GetDevice(device.put());

 auto textureName = SharedTextureName(sessionID, layerIndex, sequenceNumber);

 ID3D11Device1* d1 = nullptr;
 device->QueryInterface(&d1);
 if (!d1) {
   return false;
 }

 auto result = d1->OpenSharedResourceByName(
   textureName.c_str(),
   DXGI_SHARED_RESOURCE_READ,
   IID_PPV_ARGS(mTexture.put()));
 if (!mTexture) {
   dprintf(
     L"Failed to open shared texture {}: {:#x}",
     textureName,
     std::bit_cast<uint32_t>(result));
   return false;
 }
 const auto debugName = std::format(
   "OKB-SHM-Client-{}-{}-{}", sessionID, layerIndex, sequenceNumber);
 mTexture->SetPrivateData(
   WKPDID_D3DDebugObjectName, debugName.size(), debugName.data());
 dprintf(L"Opened shared texture {}", textureName);
 return true;
 */
}

winrt::com_ptr<ID3D11Texture2D> CreateCompatibleTexture(
  ID3D11Device* d3d,
  UINT bindFlags,
  UINT miscFlags,
  DXGI_FORMAT format) {
  D3D11_TEXTURE2D_DESC desc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = format,
    .SampleDesc = {1, 0},
    .BindFlags = bindFlags,
    .MiscFlags = miscFlags,
  };

  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(d3d->CreateTexture2D(&desc, nullptr, texture.put()));
  return std::move(texture);
}

Snapshot::Snapshot(nullptr_t) : mState(State::Empty) {
}

Snapshot::Snapshot(incorrect_kind_t) : mState(State::IncorrectKind) {
}

Snapshot::Snapshot(
  const FrameMetadata& header,
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayersTextureCache& textures,
  IPCTextureReadResources* r)
  : mLayerTextures(textures), mState(State::Empty) {
  mHeader = std::make_shared<FrameMetadata>(header);

  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "SHM::Snapshot::Snapshot()");
  const scope_guard endActivitity([&activity]() {
    TraceLoggingWriteStop(activity, "SHM::Snapshot::Snapshot()");
  });

  TraceLoggingWriteTagged(activity, "WaitForFence");
  winrt::check_hresult(ctx->Wait(fence, header.mFrameNumber));

  const D3D11_BOX box {0, 0, 0, TextureWidth, TextureHeight, 1};
  for (uint8_t i = 0; i < header.mLayerCount; ++i) {
    TraceLoggingWriteTagged(
      activity, "StartCopyTexture", TraceLoggingValue(i, "Layer"));
    ctx->CopySubresourceRegion(
      mLayerTextures.at(i)->GetD3D11Texture(),
      /* subresource = */ 0,
      /* x = */ 0,
      /* y = */ 0,
      /* z = */ 0,
      r->mLayers.at(i).mTexture.get(),
      /* subresource = */ 0,
      &box);
    TraceLoggingWriteTagged(activity, "CopiedResource");
    TraceLoggingWriteTagged(
      activity, "CopiedTexture", TraceLoggingValue(i, "Layer"));
  }
  TraceLoggingWriteTagged(activity, "Flushed");

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

bool LayerConfig::IsValid() const {
  return mImageWidth > 0 && mImageHeight > 0;
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
  if (layerIndex >= this->GetLayerCount()) {
    dprintf(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
    OPENKNEEBOARD_BREAK;
    return {};
  }

  auto config = &mHeader->mLayers[layerIndex];
  if (!config->IsValid()) {
    dprintf("Invalid config for layer {}", layerIndex);
    OPENKNEEBOARD_BREAK;
    return {};
  }

  return config;
}

bool Snapshot::IsValid() const {
  return mState == State::Valid;
}

template <lockable_state State>
constexpr bool is_valid_impl_exit_state(State state) noexcept {
  return state == State::Unlocked;
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
      DWORD {SHM_SIZE},// Perfect forwarding fails with static constexpr integer
                       // values
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
    if (!is_valid_impl_exit_state(mState.Get())) {
      dprint("Closing SHM with invalid state");
      OPENKNEEBOARD_BREAK;
      std::terminate();
    }
  }

  bool IsValid() const {
    return mMapping;
  }

  template <State in, State out>
  void Transition() {
    mState.Transition<in, out>();
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
          "Unexpected result from SHM WaitForSingleObject in lock(): {:#016x}",
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
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::unlock()");
    const auto ret = ReleaseMutex(mMutexHandle.get());
    TraceLoggingWriteStop(
      activity, "SHM::Impl::unlock()", TraceLoggingValue(ret, "Result"));
  }

 protected:
  StateMachine<State> mState = InitialState;
};

template <>
constexpr bool is_valid_impl_exit_state(WriterState state) noexcept {
  return state == WriterState::Detached;
}

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
  p->Transition<State::Unlocked, State::Detaching>();

  p->mHeader->mFlags &= ~HeaderFlags::FEEDER_ATTACHED;
  FlushViewOfFile(p->mMapping, NULL);

  p->Transition<State::Detaching, State::Detached>();
}

Writer::~Writer() {
  std::unique_lock lock(*this);
  this->Detach();
}

Writer::NextFrameInfo Writer::BeginFrame(
  [[maybe__unused]] uint8_t requestedLayerCount) noexcept {
  p->Transition<State::Locked, State::FrameInProgress>();

  const uint8_t layerCount = MaxLayers;
  const auto fenceOut = InterlockedIncrement64(&p->mHeader->mFenceValue);
  const auto fenceIn = fenceOut - 1;

  std::vector<PixelRect> layers;
  layers.reserve(layerCount);
  for (uint8_t i = 0; i < MaxLayers; ++i) {
    layers.push_back(Spriting::GetRect(i, layerCount));
  }

  return NextFrameInfo {
    .mFenceIn = fenceIn,
    .mFenceOut = fenceOut,
    .mTextureIndex
    = static_cast<uint8_t>((p->mHeader->mFrameNumber + 1) % TextureCount),
    .mLayerLocations = layers,
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
  std::array<IPCTextureReadResources, TextureCount> mResources;
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
}

Reader::operator bool() const {
  return p && p->IsValid() & p->mHeader->HaveFeeder();
}

Writer::operator bool() const {
  return (bool)p;
}

CachedReader::~CachedReader() = default;

std::shared_ptr<LayerTextureCache> CachedReader::CreateLayerTextureCache(
  [[maybe_unused]] uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::make_shared<LayerTextureCache>(texture);
}

Snapshot CachedReader::MaybeGet(
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayersTextureCache& textures,
  ConsumerKind kind) noexcept {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "SHM::MaybeGet");
  if (!*this) {
    TraceLoggingWriteStop(
      activity, "SHM::MaybeGet", TraceLoggingValue("No feeder", "Result"));
    return {nullptr};
  }

  if (
    mCache.IsValid()
    && this->GetRenderCacheKey(kind) == mCache.GetRenderCacheKey()
    && kind == mCachedConsumerKind) {
    TraceLoggingWriteStop(
      activity,
      "SHM::MaybeGet",
      TraceLoggingValue("Cache hit", "Result"),
      TraceLoggingValue(
        mCache.GetSequenceNumberForDebuggingOnly(), "SequenceNumber"));
    return mCache;
  }

  TraceLoggingWriteTagged(activity, "Waiting for SHM lock");
  const std::unique_lock shmLock(*p, std::try_to_lock);
  if (!shmLock.owns_lock()) {
    TraceLoggingWriteStop(
      activity,
      "SHM::MaybeGet",
      TraceLoggingValue("try_to_lock failed", "Result"));
    return mCache;
  }
  TraceLoggingWriteTagged(activity, "Acquired SHM lock");
  const auto newSnapshot = this->MaybeGetUncached(ctx, fence, textures, kind);

  using State = Snapshot::State;
  const auto state = newSnapshot.GetState();
  if (state == State::Empty && kind == mCachedConsumerKind) {
    TraceLoggingWriteStop(
      activity,
      "SHM::MaybeGet",
      TraceLoggingValue("Using stale cache", "Result"),
      TraceLoggingValue(
        mCache.GetSequenceNumberForDebuggingOnly(), "SequenceNumber"));
    return mCache;
  }
  if (state != State::Valid) {
    TraceLoggingWriteStop(
      activity, "SHM::MaybeGet", TraceLoggingValue("Unusable cache", "Result"));
    return newSnapshot;
  }

  const auto newSequenceNumber
    = newSnapshot.GetSequenceNumberForDebuggingOnly();
  if (newSequenceNumber < mCachedSequenceNumber) {
    TraceLoggingWriteTagged(
      activity,
      "BackwardsSequenceNumber",
      TraceLoggingValue(mCachedSequenceNumber, "previous"),
      TraceLoggingValue(newSequenceNumber, "new"));
    dprintf(
      "Sequence number went backwards! {} -> {}",
      mCachedSequenceNumber,
      newSequenceNumber);
  }

  mCache = newSnapshot;
  mCachedConsumerKind = kind;
  mCachedSequenceNumber = newSequenceNumber;
  ActiveConsumers::Set(kind);
  TraceLoggingWriteStop(
    activity,
    "SHM::MaybeGet",
    TraceLoggingValue("Updated cache", "Result"),
    TraceLoggingValue(newSequenceNumber, "SequenceNumber"),
    TraceLoggingValue(static_cast<uint8_t>(mCache.GetState()), "State"));
  return mCache;
}

Snapshot Reader::MaybeGetUncached(
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayersTextureCache& textures,
  ConsumerKind kind) const {
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

  auto& r = p->mResources.at(p->mHeader->mFrameNumber % TextureCount);
  if (!r.Populate(ctx, p->mHeader->mSessionID, p->mHeader->mFrameNumber)) {
    return {nullptr};
  }

  return Snapshot(*p->mHeader, ctx, fence, textures, &r);
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
  HANDLE fence,
  HANDLE texture) {
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
    if (layer.mImageWidth == 0 || layer.mImageHeight == 0) {
      throw std::logic_error("Not feeding a 0-size image");
    }
  }

  p->mHeader->mConfig = config;
  p->mHeader->mFrameNumber++;
  p->mHeader->mFlags |= HeaderFlags::FEEDER_ATTACHED;
  p->mHeader->mLayerCount = static_cast<uint8_t>(layers.size());
  p->mHeader->mFeederProcessID = p->mProcessID;
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

void CachedReader::InitDXResources(ID3D11Device* device) {
  if (!p) {
    return;
  }

  const auto sessionID = this->GetSessionID();
  if (mDevice == device && mSessionID == sessionID) [[likely]] {
    return;
  }
  if (!(*this)) {
    return;
  }
  winrt::com_ptr<ID3D11Device5> device5;
  winrt::check_hresult(device->QueryInterface<ID3D11Device5>(device5.put()));
  {
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(device->QueryInterface<IDXGIDevice>(dxgiDevice.put()));
    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
    DXGI_ADAPTER_DESC desc {};
    winrt::check_hresult(dxgiAdapter->GetDesc(&desc));
    dprintf(
      L"SHM reader using adapter '{}' (LUID {:#x})",
      desc.Description,
      std::bit_cast<uint64_t>(desc.AdapterLuid));
  }

  // Make sure we get a consistent view
  std::unique_lock lock(*p, std::try_to_lock);
  if (!lock.owns_lock()) {
    dprint("Failed to acquire SHM lock in InitDXResources");
    return;
  }

  mDevice = device;
  mSessionID = sessionID;

  if (!sessionID) {
    return;
  }

  for (size_t i = 0; i < mTextures.size(); ++i) {
    auto texture = SHM::CreateCompatibleTexture(
      device,
      SHM::DEFAULT_D3D11_BIND_FLAGS,
      SHM::DEFAULT_D3D11_MISC_FLAGS | D3D11_RESOURCE_MISC_SHARED
        | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
    const auto name = std::format("OKB-SHM-Texture-{}", i);
    texture->SetPrivateData(
      WKPDID_D3DDebugObjectName, name.size(), name.data());
    mTextures[i] = this->CreateLayerTextureCache(i, std::move(texture));
  }

  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());
  mContext = ctx.as<ID3D11DeviceContext4>();

  winrt::handle feeder {
    OpenProcess(PROCESS_DUP_HANDLE, FALSE, p->mHeader->mFeederProcessID)};
  if (!feeder) {
    return;
  }

  mFenceHandle = {};
  DuplicateHandle(
    feeder.get(),
    p->mHeader->mFence,
    GetCurrentProcess(),
    mFenceHandle.put(),
    0,
    FALSE,
    DUPLICATE_SAME_ACCESS);
  if (!mFenceHandle) {
    return;
  }

  mFence = {nullptr};
  device5->OpenSharedFence(mFenceHandle.get(), IID_PPV_ARGS(mFence.put()));
}

Snapshot CachedReader::MaybeGet(ID3D11Device* device, ConsumerKind kind) {
  if (!(*this)) {
    return {nullptr};
  }

  this->InitDXResources(device);

  if (!(mSessionID && mContext && mFence)) {
    return {nullptr};
  }

  return this->MaybeGet(mContext.get(), mFence.get(), mTextures, kind);
}

LayerTextureCache::LayerTextureCache(
  const winrt::com_ptr<ID3D11Texture2D>& d3d11Texture)
  : mD3D11Texture(d3d11Texture) {
}

LayerTextureCache::~LayerTextureCache() = default;

ID3D11Texture2D* LayerTextureCache::GetD3D11Texture() {
  return mD3D11Texture.get();
}

}// namespace OpenKneeboard::SHM
