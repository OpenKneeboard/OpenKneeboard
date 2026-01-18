// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard::SHM::D3D11 {

Texture::Texture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex,
  const winrt::com_ptr<ID3D11Device5>& device,
  const winrt::com_ptr<ID3D11DeviceContext4>& context)
  : IPCClientTexture(dimensions, swapchainIndex),
    mDevice(device),
    mContext(context) {
}

Texture::~Texture() = default;

ID3D11Texture2D* Texture::GetD3D11Texture() const noexcept {
  return mCacheTexture.get();
}

ID3D11ShaderResourceView* Texture::GetD3D11ShaderResourceView() noexcept {
  if (!mCacheShaderResourceView) {
    check_hresult(mDevice->CreateShaderResourceView(
      mCacheTexture.get(), nullptr, mCacheShaderResourceView.put()));
  }
  return mCacheShaderResourceView.get();
}

void Texture::CopyFrom(
  ID3D11Texture2D* sourceTexture,
  ID3D11Fence* fenceIn,
  uint64_t fenceInValue,
  ID3D11Fence* fenceOut,
  uint64_t fenceOutValue) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::Texture::CopyFrom");

  if (!mCacheTexture) {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D11/CreateCacheTexture");
    D3D11_TEXTURE2D_DESC desc;
    sourceTexture->GetDesc(&desc);
    check_hresult(
      mDevice->CreateTexture2D(&desc, nullptr, mCacheTexture.put()));
    // Will be needed within 3 frames, so never allow it to be booted from
    // VRAM to RAM
    mCacheTexture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D11/FenceIn");
    check_hresult(mContext->Wait(fenceIn, fenceInValue));
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D11/CopySubresourceRegion");
    mContext->CopySubresourceRegion(
      mCacheTexture.get(), 0, 0, 0, 0, sourceTexture, 0, nullptr);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D11/FenceOut");
    check_hresult(mContext->Signal(fenceOut, fenceOutValue));
  }
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::~CachedReader()");
  this->WaitForPendingCopies();
}

void CachedReader::WaitForPendingCopies() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::WaitForPendingCopies()");
  if (!mCopyFence) {
    return;
  }
  winrt::handle event {CreateEventEx(nullptr, nullptr, 0, GENERIC_ALL)};
  mCopyFence.mFence->SetEventOnCompletion(mCopyFence.mValue, event.get());
  WaitForSingleObject(event.get(), INFINITE);
}

void CachedReader::InitializeCache(
  ID3D11Device* device,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  if (device != mDevice.get()) {
    this->WaitForPendingCopies();
    mDevice = {};
    mDeviceContext = {};
    mCopyFence = {};

    check_hresult(device->QueryInterface(mDevice.put()));
    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    mDeviceContext = context.as<ID3D11DeviceContext4>();

    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    check_hresult(device->QueryInterface(IID_PPV_ARGS(dxgiDevice.put())));
    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
    DXGI_ADAPTER_DESC desc {};
    check_hresult(dxgiAdapter->GetDesc(&desc));

    mDeviceLUID = std::bit_cast<uint64_t>(desc.AdapterLuid);
    dprint(
      L"D3D11 SHM reader using adapter '{}' (LUID {:#x})",
      desc.Description,
      std::bit_cast<uint64_t>(mDeviceLUID));

    winrt::check_hresult(mDevice->CreateFence(
      0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(mCopyFence.mFence.put())));
  }

  SHM::CachedReader::InitializeCache(mDeviceLUID, swapchainLength);
}

void CachedReader::ReleaseIPCHandles() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::ReleaseIPCHandles");
  if (mIPCFences.empty()) {
    return;
  }
  this->WaitForPendingCopies();

  std::vector<HANDLE> events;
  for (const auto& [fenceHandle, fenceAndValue]: mIPCFences) {
    if (auto event = CreateEventEx(nullptr, nullptr, 0, GENERIC_ALL))
      [[likely]] {
      fenceAndValue.mFence->SetEventOnCompletion(fenceAndValue.mValue, event);
      events.push_back(event);
    }
  }

  WaitForMultipleObjects(events.size(), events.data(), true, INFINITE);
  for (const auto event: events) {
    CloseHandle(event);
  }

  mIPCFences.clear();
  mIPCTextures.clear();
}

void CachedReader::Copy(
  HANDLE sourceHandle,
  IPCClientTexture* destinationTexture,
  HANDLE fenceHandle,
  uint64_t fenceValueIn) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::Copy()");
  const auto source = this->GetIPCTexture(sourceHandle);

  auto fenceAndValue = this->GetIPCFence(fenceHandle);

  reinterpret_cast<SHM::D3D11::Texture*>(destinationTexture)
    ->CopyFrom(
      source,
      fenceAndValue->mFence.get(),
      fenceValueIn,
      mCopyFence.mFence.get(),
      ++mCopyFence.mValue);
  fenceAndValue->mValue = fenceValueIn;
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D11::Texture>(
    dimensions, swapchainIndex, mDevice, mDeviceContext);
}

CachedReader::FenceAndValue* CachedReader::GetIPCFence(HANDLE handle) noexcept {
  if (mIPCFences.contains(handle)) {
    return &mIPCFences.at(handle);
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::GetIPCFence()");
  winrt::com_ptr<ID3D11Fence> fence;
  check_hresult(mDevice->OpenSharedFence(handle, IID_PPV_ARGS(fence.put())));
  mIPCFences.emplace(handle, FenceAndValue {fence});
  return &mIPCFences.at(handle);
}

ID3D11Texture2D* CachedReader::GetIPCTexture(HANDLE handle) noexcept {
  if (mIPCTextures.contains(handle)) {
    return mIPCTextures.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::GetIPCTexture()");

  winrt::com_ptr<ID3D11Texture2D> texture;
  check_hresult(
    mDevice->OpenSharedResource1(handle, IID_PPV_ARGS(texture.put())));
  mIPCTextures.emplace(handle, texture);
  return texture.get();
}

}// namespace OpenKneeboard::SHM::D3D11