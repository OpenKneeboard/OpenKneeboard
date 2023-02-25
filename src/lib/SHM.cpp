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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/bitflags.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>
#include <Windows.h>
#include <d3d11_2.h>
#include <d3d11_3.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <processthreadsapi.h>

#include <bit>
#include <format>
#include <random>
#include <shims/utility>

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

struct Header final {
  uint32_t mSequenceNumber = 0;
  uint64_t mSessionID = CreateSessionID();
  HeaderFlags mFlags;
  Config mConfig;

  DWORD mFeederProcessID {};
  HANDLE mFence {};

  uint8_t mLayerCount = 0;
  LayerConfig mLayers[MaxLayers];

  size_t GetRenderCacheKey() const;
};
static_assert(std::is_standard_layout_v<Header>);

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<SHM::HeaderFlags> = true;
}// namespace OpenKneeboard

namespace OpenKneeboard::SHM {

static constexpr DWORD MAX_IMAGE_PX(1024 * 1024 * 8);
static constexpr DWORD SHM_SIZE = sizeof(Header);

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

struct LayerTextureReadResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;

  void Populate(
    ID3D11DeviceContext* ctx,
    uint64_t sessionID,
    uint8_t layerIndex,
    uint32_t sequenceNumber);
};

struct TextureReadResources {
  uint64_t mSessionID = 0;

  std::array<LayerTextureReadResources, MaxLayers> mLayers;

  void Populate(
    ID3D11DeviceContext* ctx,
    uint64_t sessionID,
    uint32_t sequenceNumber);
};

void TextureReadResources::Populate(
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
    mLayers[i].Populate(ctx, sessionID, i, sequenceNumber);
  }
}

void LayerTextureReadResources::Populate(
  ID3D11DeviceContext* ctx,
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber) {
  if (mTexture) {
    return;
  }

  winrt::com_ptr<ID3D11Device> device;
  ctx->GetDevice(device.put());

  auto textureName
    = SHM::SharedTextureName(sessionID, layerIndex, sequenceNumber);

  ID3D11Device1* d1 = nullptr;
  device->QueryInterface(&d1);
  if (!d1) {
    return;
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
    return;
  }
  dprintf(L"Opened shared texture {}", textureName);
}

std::wstring SharedTextureName(
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber) {
  return std::format(
    L"Local\\{}-{}.{}.{}.{}--texture-s{:x}-l{}-b{}",
    ProjectNameW,
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    sessionID,
    layerIndex,
    sequenceNumber % TextureCount);
}

winrt::com_ptr<ID3D11Texture2D>
CreateCompatibleTexture(ID3D11Device* d3d, UINT bindFlags, UINT miscFlags) {
  D3D11_TEXTURE2D_DESC desc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = bindFlags,
    .MiscFlags = miscFlags,
  };

  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(d3d->CreateTexture2D(&desc, nullptr, texture.put()));
  return std::move(texture);
}

Snapshot::Snapshot() {
}

Snapshot::Snapshot(
  const Header& header,
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayerTextures& textures,
  TextureReadResources* r)
  : mLayerTextures(textures) {
  mHeader = std::make_shared<Header>(header);

  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "SHM::Snapshot::Snapshot()");
  const scope_guard endActivitity([&activity]() {
    TraceLoggingWriteStop(activity, "SHM::Snapshot::Snapshot()");
  });

  TraceLoggingWriteTagged(activity, "Wait for fence");
  winrt::check_hresult(ctx->Wait(fence, header.mSequenceNumber));

  const D3D11_BOX box {0, 0, 0, TextureWidth, TextureHeight, 1};
  for (uint8_t i = 0; i < this->GetLayerCount(); ++i) {
    TraceLoggingWriteTagged(
      activity, "Start copy texture", TraceLoggingValue(i, "Layer"));
    ctx->CopyResource(
      mLayerTextures.at(i).get(), r->mLayers.at(i).mTexture.get());
    TraceLoggingWriteTagged(activity, "Copied resource");
    TraceLoggingWriteTagged(
      activity, "Copied texture", TraceLoggingValue(i, "Layer"));
  }
  ctx->Flush();
  TraceLoggingWriteTagged(activity, "Flushed");
  mLayerSRVs = std::make_shared<LayerSRVArray>();
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
  return mHeader->mSequenceNumber;
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

winrt::com_ptr<ID3D11Texture2D> Snapshot::GetLayerTexture(
  ID3D11Device* d3d,
  uint8_t layerIndex) const {
  if (layerIndex >= this->GetLayerCount()) {
    dprintf(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
    OPENKNEEBOARD_BREAK;
    return {};
  }

  return mLayerTextures.at(layerIndex);
}

winrt::com_ptr<ID3D11ShaderResourceView> Snapshot::GetLayerShaderResourceView(
  ID3D11Device* d3d,
  uint8_t layerIndex) const {
  if (layerIndex >= this->GetLayerCount()) {
    dprintf(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
    OPENKNEEBOARD_BREAK;
    return {};
  }

  auto& srv = (*mLayerSRVs).at(layerIndex);
  if (!srv) {
    winrt::check_hresult(d3d->CreateShaderResourceView(
      mLayerTextures.at(layerIndex).get(), nullptr, srv.put()));
  }

  return srv;
}

bool Snapshot::IsValid() const {
  return mHeader
    && static_cast<bool>(mHeader->mFlags & HeaderFlags::FEEDER_ATTACHED)
    && mHeader->mLayerCount > 0;
}

class Impl {
 public:
  winrt::handle mFileHandle;
  winrt::handle mMutexHandle;
  std::byte* mMapping = nullptr;
  Header* mHeader = nullptr;

  Impl() {
    winrt::handle fileHandle {CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      0,
      SHM_SIZE,
      SHMPath().c_str())};
    if (!fileHandle) {
      dprintf(
        "CreateFileMappingW failed: {}", static_cast<int>(GetLastError()));
      return;
    }

    winrt::handle mutexHandle {
      CreateMutexW(nullptr, FALSE, MutexPath().c_str())};
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
    mHeader = reinterpret_cast<Header*>(mMapping);
  }

  ~Impl() {
    UnmapViewOfFile(mMapping);
    if (mHaveLock) {
      dprint("Closing SHM while holding lock!");
      OPENKNEEBOARD_BREAK;
      std::terminate();
    }
  }

  bool IsValid() const {
    return mMapping;
  }

  bool HaveLock() const {
    return mHaveLock;
  }

  // "Lockable" C++ named concept: supports std::unique_lock

  void lock() {
    if (mHaveLock) {
      throw std::logic_error("Acquiring a lock twice");
    }
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::lock()");

    const auto result = WaitForSingleObject(mMutexHandle.get(), INFINITE);
    if (result != WAIT_OBJECT_0) {
      TraceLoggingWriteStop(
        activity, "SHM::Impl::lock()", TraceLoggingValue(result, "Error"));
      dprintf(
        "Unexpected result from SHM WaitForSingleObject in lock(): {:#016x}",
        static_cast<uint64_t>(result));
      OPENKNEEBOARD_BREAK;
      return;
    }
    TraceLoggingWriteStop(
      activity, "SHM::Impl::lock()", TraceLoggingValue("Success", "Result"));
    mHaveLock = true;
  }

  bool try_lock() {
    if (mHaveLock) {
      throw std::logic_error("Acquiring a lock twice");
    }
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::try_lock()");

    const auto result = WaitForSingleObject(mMutexHandle.get(), 0);
    if (result != WAIT_OBJECT_0) {
      TraceLoggingWriteStop(
        activity, "SHM::Impl::try_lock()", TraceLoggingValue(result, "Error"));
      dprintf(
        "Unexpected result from SHM WaitForSingleObject in try_lock(): "
        "{:#016x}",
        static_cast<uint64_t>(result));
      return false;
    }
    mHaveLock = true;
    TraceLoggingWriteStop(
      activity,
      "SHM::Impl::try_lock()",
      TraceLoggingValue("Success", "Result"));
    return true;
  }

  void unlock() {
    if (!mHaveLock) {
      throw std::logic_error("Can't release a lock we don't hold");
    }
    TraceLoggingThreadActivity<gTraceProvider> activity;
    TraceLoggingWriteStart(activity, "SHM::Impl::unlock()");
    const auto ret = ReleaseMutex(mMutexHandle.get());
    TraceLoggingWriteStop(
      activity, "SHM::Impl::unlock()", TraceLoggingValue(ret, "Result"));
    mHaveLock = false;
  }

 private:
  bool mHaveLock = false;
};

class Writer::Impl : public SHM::Impl {
 public:
  using SHM::Impl::Impl;
  bool mHaveFed = false;
  DWORD mProcessID = GetCurrentProcessId();

  ~Impl() {
    mHeader->mFlags &= ~HeaderFlags::FEEDER_ATTACHED;
    FlushViewOfFile(mMapping, NULL);
  }
};

Writer::Writer() {
  const auto path = SHMPath();
  dprintf(L"Initializing SHM writer with path {}", path);

  p = std::make_shared<Impl>();
  if (!p->IsValid()) {
    p.reset();
    return;
  }

  *p->mHeader = Header {};
  dprint("Writer initialized.");
}

Writer::~Writer() {
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

UINT Writer::GetNextTextureIndex() const {
  return (p->mHeader->mSequenceNumber + 1) % TextureCount;
}

uint64_t Writer::GetSessionID() const {
  return p->mHeader->mSessionID;
}

uint32_t Writer::GetNextSequenceNumber() const {
  return p->mHeader->mSequenceNumber + 1;
}

class Reader::Impl : public SHM::Impl {
 public:
  std::array<TextureReadResources, TextureCount> mResources;
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
  return p
    && static_cast<bool>(p->mHeader->mFlags & HeaderFlags::FEEDER_ATTACHED);
}

Writer::operator bool() const {
  return (bool)p;
}

Snapshot Reader::MaybeGet(
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayerTextures& textures,
  ConsumerKind kind) noexcept {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "SHM::MaybeGet");
  if (!*this) {
    TraceLoggingWriteStop(
      activity, "SHM::MaybeGet", TraceLoggingValue("No feeder", "Result"));
    return {};
  }

  if (
    mCache.IsValid() && this->GetRenderCacheKey() == mCache.GetRenderCacheKey()
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

  if (!newSnapshot.IsValid()) {
    if (kind == mCachedConsumerKind) {
      TraceLoggingWriteStop(
        activity,
        "SHM::MaybeGet",
        TraceLoggingValue("Using stale cache", "Result"),
        TraceLoggingValue(
          mCache.GetSequenceNumberForDebuggingOnly(), "SequenceNumber"));
      return mCache;
    }
    TraceLoggingWriteStop(
      activity, "SHM::MaybeGet", TraceLoggingValue("Unusable cache", "Result"));
    return {};
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
  TraceLoggingWriteStop(
    activity,
    "SHM::MaybeGet",
    TraceLoggingValue("Updated cache", "Result"),
    TraceLoggingValue(newSequenceNumber, "Sequence number"));
  return mCache;
}

Snapshot Reader::MaybeGetUncached(
  ID3D11DeviceContext4* ctx,
  ID3D11Fence* fence,
  const LayerTextures& textures,
  ConsumerKind kind) const {
  if (!p->HaveLock()) {
    dprint("Can't get without lock");
    throw std::logic_error("Reader::MaybeGet() without lock");
  }

  if (!p->mHeader->mConfig.mTarget.Matches(kind)) {
    dprint("Kind mismatch, not returning new snapshot");
    return {};
  }

  auto& r = p->mResources.at(p->mHeader->mSequenceNumber % TextureCount);
  r.Populate(ctx, p->mHeader->mSessionID, p->mHeader->mSequenceNumber);

  return Snapshot(*p->mHeader, ctx, fence, textures, &r);
}

size_t Reader::GetRenderCacheKey() const {
  if (!(p && p->mHeader)) {
    return {};
  }
  return p->mHeader->GetRenderCacheKey();
}

void Writer::Update(
  const Config& config,
  const std::vector<LayerConfig>& layers,
  HANDLE fence) {
  if (!p) {
    throw std::logic_error("Attempted to update invalid SHM");
  }
  if (!p->HaveLock()) {
    throw std::logic_error("Attempted to update SHM without a lock");
  }

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
  p->mHeader->mSequenceNumber++;
  p->mHeader->mFlags |= HeaderFlags::FEEDER_ATTACHED;
  p->mHeader->mLayerCount = static_cast<uint8_t>(layers.size());
  p->mHeader->mFeederProcessID = p->mProcessID;
  p->mHeader->mFence = fence;
  memcpy(
    p->mHeader->mLayers, layers.data(), sizeof(LayerConfig) * layers.size());
}

size_t Header::GetRenderCacheKey() const {
  // This is lazy, and only works because:
  // - session ID already contains random data
  // - we're only combining *one* other value which isn't
  // If adding more data, it either needs to be random,
  // or need something like boost::hash_combine()
  std::hash<uint64_t> HashUI64;
  return HashUI64(mSessionID) ^ HashUI64(mSequenceNumber);
}

uint32_t Reader::GetFrameCountForMetricsOnly() const {
  if (!(p && p->mHeader)) {
    return {};
  }
  return p->mHeader->mSequenceNumber;
}

ConsumerPattern::ConsumerPattern() = default;
ConsumerPattern::ConsumerPattern(
  std::underlying_type_t<ConsumerKind> consumerKindMask)
  : mKindMask(consumerKindMask) {
}

bool ConsumerPattern::Matches(ConsumerKind kind) const {
  return (mKindMask & std23::to_underlying(kind)) == mKindMask;
}

void SingleBufferedReader::InitDXResources(ID3D11Device* device) {
  const auto sessionID = this->GetSessionID();
  if (mDevice == device && mSessionID == sessionID) [[likely]] {
    return;
  }
  winrt::com_ptr<ID3D11Device5> device5;
  winrt::check_hresult(device->QueryInterface<ID3D11Device5>(device5.put()));

  mDevice = device;
  mSessionID = sessionID;

  if (!sessionID) {
    return;
  }

  for (auto& t: mTextures) {
    t = SHM::CreateCompatibleTexture(device);
  }

  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());
  mContext = ctx.as<ID3D11DeviceContext4>();

  winrt::handle feeder {
    OpenProcess(PROCESS_DUP_HANDLE, FALSE, p->mHeader->mFeederProcessID)};
  mFenceHandle = {};
  winrt::check_hresult(DuplicateHandle(
    feeder.get(),
    p->mHeader->mFence,
    GetCurrentProcess(),
    mFenceHandle.put(),
    0,
    FALSE,
    DUPLICATE_SAME_ACCESS));

  mFence = {nullptr};
  winrt::check_hresult(
    device5->OpenSharedFence(mFenceHandle.get(), IID_PPV_ARGS(mFence.put())));
}

Snapshot SingleBufferedReader::MaybeGet(
  ID3D11Device* device,
  ConsumerKind kind) {
  this->InitDXResources(device);

  if (!(mSessionID && mContext && mFence)) {
    return {};
  }

  return Reader::MaybeGet(mContext.get(), mFence.get(), mTextures, kind);
}

}// namespace OpenKneeboard::SHM
