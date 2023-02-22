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
#include <OpenKneeboard/version.h>
#include <Windows.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

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
  winrt::com_ptr<IDXGIKeyedMutex> mMutex;

  void Populate(
    ID3D11Device* d3d,
    uint64_t sessionID,
    uint8_t layerIndex,
    uint32_t sequenceNumber);
};

struct TextureReadResources {
  uint64_t mSessionID = 0;

  std::array<LayerTextureReadResources, MaxLayers> mLayers;

  void Populate(ID3D11Device* d3d, uint64_t sessionID, uint32_t sequenceNumber);
};

void TextureReadResources::Populate(
  ID3D11Device* d3d,
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
    mLayers[i].Populate(d3d, sessionID, i, sequenceNumber);
  }
}

void LayerTextureReadResources::Populate(
  ID3D11Device* d3d,
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber) {
  if (mTexture) {
    return;
  }

  auto textureName
    = SHM::SharedTextureName(sessionID, layerIndex, sequenceNumber);

  ID3D11Device1* d1 = nullptr;
  d3d->QueryInterface(&d1);
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
  mMutex = mTexture.as<IDXGIKeyedMutex>();
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
  ID3D11Device* d3d,
  TextureReadResources* r) {
  mHeader = std::make_shared<Header>(header);

  winrt::com_ptr<ID3D11DeviceContext> ctx;
  d3d->GetImmediateContext(ctx.put());

  const D3D11_BOX box {0, 0, 0, TextureWidth, TextureHeight, 1};
  for (uint8_t i = 0; i < this->GetLayerCount(); ++i) {
    auto& layer = r->mLayers[i];
    auto& t = mLayerTextures.at(i);
    t = SHM::CreateCompatibleTexture(d3d);
    winrt::check_hresult(layer.mMutex->AcquireSync(0, INFINITE));
    ctx->CopyResource(t.get(), layer.mTexture.get());
    winrt::check_hresult(layer.mMutex->ReleaseSync(0));
  }
  ctx->Flush();
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
    const auto result = WaitForSingleObject(mMutexHandle.get(), INFINITE);
    if (result != WAIT_OBJECT_0) {
      dprintf(
        "Unexpected result from SHM WaitForSingleObject in lock(): {:#016x}",
        static_cast<uint64_t>(result));
      OPENKNEEBOARD_BREAK;
      return;
    }
    mHaveLock = true;
  }

  bool try_lock() {
    if (mHaveLock) {
      throw std::logic_error("Acquiring a lock twice");
    }
    const auto result = WaitForSingleObject(mMutexHandle.get(), INFINITE);
    if (result != WAIT_OBJECT_0) {
      dprintf(
        "Unexpected result from SHM WaitForSingleObject in try_lock(): "
        "{:#016x}",
        static_cast<uint64_t>(result));
      return false;
    }
    mHaveLock = true;
    return true;
  }

  void unlock() {
    if (!mHaveLock) {
      throw std::logic_error("Can't release a lock we don't hold");
    }
    ReleaseMutex(mMutexHandle.get());
    mHaveLock = false;
  }

 private:
  bool mHaveLock = false;
};

class Writer::Impl : public SHM::Impl {
 public:
  using SHM::Impl::Impl;
  bool mHaveFed = false;

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

Snapshot Reader::MaybeGet(ID3D11Device* d3d, ConsumerKind kind) {
  if (!*this) {
    return {};
  }

  if (
    mCache.IsValid() && this->GetRenderCacheKey() == mCache.GetRenderCacheKey()
    && kind == mCachedConsumerKind) {
    return mCache;
  }

  const std::unique_lock shmLock(*p);
  const auto newSnapshot = this->MaybeGetUncached(d3d, kind);

  if (!newSnapshot.IsValid()) {
    if (kind == mCachedConsumerKind) {
      return mCache;
    }
    return {};
  }

  const auto newSequenceNumber
    = newSnapshot.GetSequenceNumberForDebuggingOnly();
  if (newSequenceNumber < mCachedSequenceNumber) {
    dprintf(
      "Sequence number went backwards! {} -> {}",
      mCachedSequenceNumber,
      newSequenceNumber);
  }

  mCache = newSnapshot;
  mCachedConsumerKind = kind;
  mCachedSequenceNumber = newSequenceNumber;
  return mCache;
}

Snapshot Reader::MaybeGetUncached(ID3D11Device* d3d, ConsumerKind kind) const {
  if (!p->HaveLock()) {
    dprint("Can't get without lock");
    throw std::logic_error("Reader::MaybeGet() without lock");
  }

  if (!p->mHeader->mConfig.mTarget.Matches(kind)) {
    dprint("Kind mismatch, not returning new snapshot");
    return {};
  }

  auto& r = p->mResources.at(p->mHeader->mSequenceNumber % TextureCount);
  r.Populate(d3d, p->mHeader->mSessionID, p->mHeader->mSequenceNumber);

  return Snapshot(*p->mHeader, d3d, &r);
}

size_t Reader::GetRenderCacheKey() const {
  if (!(p && p->mHeader)) {
    return {};
  }
  return p->mHeader->GetRenderCacheKey();
}

void Writer::Update(
  const Config& config,
  const std::vector<LayerConfig>& layers) {
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

}// namespace OpenKneeboard::SHM
