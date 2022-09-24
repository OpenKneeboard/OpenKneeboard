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
#include <OpenKneeboard/bitflags.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/shm.h>
#include <OpenKneeboard/version.h>
#include <Windows.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

#include <bit>
#include <format>
#include <random>

namespace OpenKneeboard::SHM {

static uint64_t CreateSessionID() {
  std::random_device randDevice;
  std::uniform_int_distribution<uint32_t> randDist;
  return (static_cast<uint64_t>(GetCurrentProcessId()) << 32)
    | randDist(randDevice);
}

enum class HeaderFlags : ULONG {
  LOCKED = 1 << 0,
  FEEDER_ATTACHED = 1 << 1,
};

#pragma pack(push)
struct Header final {
  uint32_t mSequenceNumber = 0;
  uint64_t mSessionID = CreateSessionID();
  HeaderFlags mFlags;
  Config mConfig;

  uint8_t mLayerCount = 0;
  LayerConfig mLayers[MaxLayers];

  size_t GetRenderCacheKey() const;
};
#pragma pack(pop)

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<SHM::HeaderFlags> = true;
}// namespace OpenKneeboard

namespace OpenKneeboard::SHM {

namespace {

class Spinlock final {
 private:
  Header* mHeader = nullptr;

 public:
  enum FAILURE_BEHAVIOR {
    ON_FAILURE_FORCE_UNLOCK,
    ON_FAILURE_CREATE_FALSEY,
    ON_FAILURE_THROW_EXCEPTION
  };

  Spinlock(Header* header, FAILURE_BEHAVIOR behavior) : mHeader(header) {
    const auto bit = std::countr_zero(static_cast<ULONG>(HeaderFlags::LOCKED));
    static_assert(static_cast<ULONG>(HeaderFlags::LOCKED) == 1 << bit);

    for (int count = 1; count <= 1000; ++count) {
      auto alreadyLocked
        = _interlockedbittestandset(std::bit_cast<LONG*>(&header->mFlags), bit);
      if (!alreadyLocked) {
        return;
      }
      if (count % 10 == 0) {
        YieldProcessor();
      }
      if (count % 100 == 0) {
        Sleep(10 /* ms */);
      }
    }

    if (behavior == ON_FAILURE_FORCE_UNLOCK) {
      return;
    }

    if (behavior == ON_FAILURE_CREATE_FALSEY) {
      mHeader = nullptr;
      return;
    }

    throw new std::runtime_error("Failed to acquire spinlock");
  }

  operator bool() {
    return (bool)mHeader;
  }

  ~Spinlock() {
    if (!mHeader) {
      return;
    }

    mHeader->mFlags &= ~HeaderFlags::LOCKED;
  }
};

constexpr DWORD MAX_IMAGE_PX(1024 * 1024 * 8);
constexpr DWORD SHM_SIZE = sizeof(Header);

auto SHMPath() {
  static std::wstring sCache;
  if (!sCache.empty()) {
    return sCache;
  }
  sCache = std::format(
    L"{}/{}.{}.{}.{}-{}-s{:x}",
    ProjectNameW,
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    Version::CommitIDW.substr(0, 7),
    SHM_SIZE);
  return sCache;
}

UINT GetTextureKeyFromSequenceNumber(uint32_t sequenceNumber) {
  return (sequenceNumber % (std::numeric_limits<UINT>::max() - 1)) + 1;
}

}// namespace

struct LayerTextureReadResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<IDXGISurface> mSurface;
  winrt::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
  winrt::com_ptr<IDXGIKeyedMutex> mMutex;

  void Populate(
    ID3D11Device* d3d,
    uint64_t sessionID,
    uint8_t layerIndex,
    uint32_t sequenceNumber);
};

struct TextureReadResources {
  uint64_t mSessionID = 0;

  LayerTextureReadResources mLayers[MaxLayers];

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
  dprintf(L"Opened shared texture {}", textureName);

  mSurface = mTexture.as<IDXGISurface>();
  mMutex = mTexture.as<IDXGIKeyedMutex>();

  d1->CreateShaderResourceView(
    mTexture.get(), nullptr, mShaderResourceView.put());
}

std::wstring SharedTextureName(
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber) {
  return std::format(
    L"Local\\{}-{}.{}.{}.{}-{}-texture-s{:x}-l{}-b{}",
    ProjectNameW,
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    Version::CommitIDW.substr(0, 7),
    sessionID,
    layerIndex,
    sequenceNumber % TextureCount);
}

SharedTexture11::SharedTexture11() {
}

SharedTexture11::SharedTexture11(SharedTexture11&& other)
  : mKey(other.mKey), mResources(other.mResources) {
  other.mKey = 0;
  other.mResources = nullptr;
}

SharedTexture11::SharedTexture11(
  const Header& header,
  ID3D11Device* d3d,
  uint8_t layerIndex,
  TextureReadResources* r) {
  r->Populate(d3d, header.mSessionID, header.mSequenceNumber);
  auto l = &r->mLayers[layerIndex];
  if (!l->mMutex) {
    return;
  }

  auto key = GetTextureKeyFromSequenceNumber(header.mSequenceNumber);
  if (l->mMutex->AcquireSync(key, 10) != S_OK) {
    return;
  }
  mResources = l;
  mKey = key;
}

SharedTexture11::~SharedTexture11() {
  if (mKey == 0) {
    return;
  }
  if (!mResources) {
    return;
  }
  mResources->mMutex->ReleaseSync(mKey);
}

bool SharedTexture11::IsValid() const {
  return mResources && mKey != 0;
}

ID3D11Texture2D* SharedTexture11::GetTexture() const {
  return mResources->mTexture.get();
}

IDXGISurface* SharedTexture11::GetSurface() const {
  return mResources->mSurface.get();
}

ID3D11ShaderResourceView* SharedTexture11::GetShaderResourceView() const {
  return mResources->mShaderResourceView.get();
}

Snapshot::Snapshot() {
}

Snapshot::Snapshot(const Header& header, TextureReadResources* r)
  : mResources(r) {
  mHeader = std::make_unique<Header>(header);
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
  return this && mImageWidth > 0 && mImageHeight > 0;
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

SharedTexture11 Snapshot::GetLayerTexture(ID3D11Device* d3d, uint8_t layerIndex)
  const {
  if (layerIndex >= this->GetLayerCount()) {
    dprintf(
      "Asked for layer {}, but there are {} layers",
      layerIndex,
      this->GetLayerCount());
    OPENKNEEBOARD_BREAK;
    return {};
  }

  SharedTexture11 texture(*mHeader, d3d, layerIndex, mResources);
  if (!texture.IsValid()) {
    return {};
  }

  return std::move(texture);
}

bool Snapshot::IsValid() const {
  return mHeader
    && static_cast<bool>(mHeader->mFlags & HeaderFlags::FEEDER_ATTACHED)
    && mHeader->mLayerCount > 0;
}

class Impl {
 public:
  winrt::handle mHandle;
  std::byte* mMapping = nullptr;
  Header* mHeader = nullptr;

  ~Impl() {
    UnmapViewOfFile(mMapping);
  }
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

  winrt::handle handle {CreateFileMappingW(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str())};
  if (!handle) {
    dprintf(
      "CreateFileMappingW failed: {:#x}",
      std::bit_cast<uint32_t>(GetLastError()));
    return;
  }
  auto mapping = reinterpret_cast<std::byte*>(
    MapViewOfFile(handle.get(), FILE_MAP_WRITE, 0, 0, SHM_SIZE));
  if (!mapping) {
    dprintf(
      "MapViewOfFile failed: {:#x}", std::bit_cast<uint32_t>(GetLastError()));
    return;
  }

  p = std::make_shared<Impl>();
  p->mHandle = std::move(handle);
  p->mMapping = mapping;
  p->mHeader = reinterpret_cast<Header*>(mapping);
  *p->mHeader = {};
  dprint("Writer initialized.");
}

Writer::~Writer() {
}

UINT Writer::GetPreviousTextureKey() const {
  return GetTextureKeyFromSequenceNumber(p->mHeader->mSequenceNumber);
}

UINT Writer::GetNextTextureKey() const {
  return GetTextureKeyFromSequenceNumber(p->mHeader->mSequenceNumber + 1);
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
  std::vector<TextureReadResources> mResources;
};

Reader::Reader() {
  const auto path = SHMPath();
  dprintf(L"Initializing SHM reader with path {}", path);
  winrt::handle handle {CreateFileMappingW(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str())};
  if (!handle) {
    dprintf(
      "CreateFileMappingW failed: {:#x}",
      std::bit_cast<uint32_t>(GetLastError()));
    return;
  }
  auto mapping = reinterpret_cast<std::byte*>(
    MapViewOfFile(handle.get(), FILE_MAP_WRITE, 0, 0, SHM_SIZE));
  if (!mapping) {
    dprintf(
      "MapViewOfFile failed: {:#x}", std::bit_cast<uint32_t>(GetLastError()));
    return;
  }

  this->p = std::make_shared<Impl>();
  p->mHandle = std::move(handle);
  p->mMapping = mapping;
  p->mHeader = reinterpret_cast<Header*>(mapping);
  p->mResources = {TextureCount};
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

Snapshot Reader::MaybeGet(ConsumerKind kind) const {
  Spinlock lock(p->mHeader, Spinlock::ON_FAILURE_CREATE_FALSEY);
  if (!lock) {
    return {};
  }

  if (!p->mHeader->mConfig.mTarget.Matches(kind)) {
    return {};
  }

  return Snapshot(
    *p->mHeader, &p->mResources.at(p->mHeader->mSequenceNumber % TextureCount));
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

  if (layers.size() > MaxLayers) {
    throw std::logic_error(std::format(
      "Asked to publish {} layers, but max is {}", layers.size(), MaxLayers));
  }

  for (auto layer: layers) {
    if (layer.mImageWidth == 0 || layer.mImageHeight == 0) {
      throw std::logic_error("Not feeding a 0-size image");
    }
  }

  Spinlock lock(p->mHeader, Spinlock::ON_FAILURE_FORCE_UNLOCK);
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
  return (mKindMask & static_cast<std::underlying_type_t<ConsumerKind>>(kind))
    == mKindMask;
}

}// namespace OpenKneeboard::SHM
