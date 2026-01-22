// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard::SHM::D3D11 {

namespace {
[[nodiscard]]
auto GetLUID(ID3D11Device* device) {
  auto dxgiDevice = wil::com_query<IDXGIDevice>(device);
  wil::com_ptr<IDXGIAdapter> adapter;
  check_hresult(dxgiDevice->GetAdapter(adapter.put()));
  DXGI_ADAPTER_DESC desc {};
  check_hresult(adapter->GetDesc(&desc));
  return std::bit_cast<uint64_t>(desc.AdapterLuid);
}
}// namespace

Reader::Reader(const ConsumerKind kind, ID3D11Device* const device)
  : SHM::Reader(kind, GetLUID(device)) {
  wil::com_query_to(device, mDevice.put());
}

Reader::~Reader() = default;

Frame Reader::Map(SHM::Frame raw) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::Reader::Map");

  auto& com = mFrames.at(raw.mIndex);
  if (com.mTextureHandle != raw.mTexture) {
    com.mTextureHandle = raw.mTexture;
    com.mTexture.reset();
    com.mShaderResourceView.reset();
  }
  if (com.mFenceHandle != raw.mFence) {
    com.mFenceHandle = raw.mFence;
    com.mFence.reset();
  }

  if (!com.mTexture) {
    check_hresult(mDevice->OpenSharedResource1(
      com.mTextureHandle, IID_PPV_ARGS(com.mTexture.put())));
  }
  if (!com.mShaderResourceView) {
    check_hresult(mDevice->CreateShaderResourceView(
      com.mTexture.get(), nullptr, com.mShaderResourceView.put()));
  }
  if (!com.mFence) {
    check_hresult(mDevice->OpenSharedFence(
      com.mFenceHandle, IID_PPV_ARGS(com.mFence.put())));
  }

  return Frame {
    .mConfig = std::move(raw.mConfig),
    .mLayers = std::move(raw.mLayers),
    .mTexture = com.mTexture.get(),
    .mShaderResourceView = com.mShaderResourceView.get(),
    .mFence = com.mFence.get(),
    .mFenceIn = std::bit_cast<uint64_t>(raw.mFenceIn),
  };
}
std::expected<Frame, Frame::Error> Reader::MaybeGetMapped() {
  auto raw = MaybeGet();
  if (!raw) {
    return std::unexpected {raw.error()};
  }
  return Map(*std::move(raw));
}

void Reader::OnSessionChanged() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::Reader::OnSessionChanged");
  mFrames = {};
}
}// namespace OpenKneeboard::SHM::D3D11
