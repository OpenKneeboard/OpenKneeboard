#include "NonVRD3D11Kneeboard.h"

#include <DirectXTK/SpriteBatch.h>
#include <DirectXTK/WICTextureLoader.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <dxgi.h>
#include <winrt/base.h>

#include "InjectedDLLMain.h"

namespace OpenKneeboard {

struct NonVRD3D11Kneeboard::Impl {
  SHM::Reader shm;
};

NonVRD3D11Kneeboard::NonVRD3D11Kneeboard() : p(std::make_unique<Impl>()) {
  IDXGISwapChainPresentHook::InitWithVTable();
}

NonVRD3D11Kneeboard::~NonVRD3D11Kneeboard() {
  this->UninstallHook();
}

void NonVRD3D11Kneeboard::UninstallHook() {
  IDXGISwapChainPresentHook::UninstallHook();
}

HRESULT NonVRD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) {
  auto shm = p->shm.MaybeGet();
  if (!shm) {
    return std::invoke(next, swapChain, syncInterval, flags);
  }

  const auto& header = *shm.GetHeader();

  winrt::com_ptr<ID3D11Device> device;
  swapChain->GetDevice(IID_PPV_ARGS(device.put()));
  winrt::com_ptr<ID3D11DeviceContext> context;
  device->GetImmediateContext(context.put());

  static_assert(sizeof(SHM::Pixel) == 4, "Expecting B8G8R8A8 for DirectX");
  static_assert(offsetof(SHM::Pixel, b) == 0, "Expected blue to be first byte");
  static_assert(offsetof(SHM::Pixel, a) == 3, "Expected alpha to be last byte");
  winrt::com_ptr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC desc {
    .Width = header.imageWidth,
    .Height = header.imageHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = {},
    .MiscFlags = {},
  };
  D3D11_SUBRESOURCE_DATA initialData {
    shm.GetPixels(), header.imageWidth * sizeof(SHM::Pixel), 0};
  device->CreateTexture2D(&desc, &initialData, texture.put());
  winrt::com_ptr<ID3D11ShaderResourceView> resourceView;
  device->CreateShaderResourceView(texture.get(), nullptr, resourceView.put());

  DXGI_SWAP_CHAIN_DESC scDesc;
  swapChain->GetDesc(&scDesc);

  const auto aspectRatio = float(header.imageWidth) / header.imageHeight;
  const LONG canvasWidth = scDesc.BufferDesc.Width;
  const LONG canvasHeight = scDesc.BufferDesc.Height;

  const LONG renderHeight
    = (static_cast<long>(canvasHeight) * header.flat.heightPercent) / 100;
  const LONG renderWidth = std::lround(renderHeight * aspectRatio);

  const auto padding = header.flat.paddingPixels;

  LONG left = padding;
  switch (header.flat.horizontalAlignment) {
    case SHM::FlatConfig::HALIGN_LEFT:
      break;
    case SHM::FlatConfig::HALIGN_CENTER:
      left = (canvasWidth - renderWidth) / 2;
      break;
    case SHM::FlatConfig::HALIGN_RIGHT:
      left = canvasWidth - (renderWidth + padding);
      break;
  }

  LONG top = padding;
  switch (header.flat.verticalAlignment) {
    case SHM::FlatConfig::VALIGN_TOP:
      break;
    case SHM::FlatConfig::VALIGN_MIDDLE:
      top = (canvasHeight - renderHeight) / 2;
      break;
    case SHM::FlatConfig::VALIGN_BOTTOM:
      top = canvasHeight - (renderHeight + padding);
      break;
  }

  DirectX::SpriteBatch sprites(context.get());
  sprites.Begin();
  sprites.Draw(
    resourceView.get(),
    RECT {left, top, left + renderWidth, top + renderHeight},
    DirectX::Colors::White * header.flat.opacity);
  sprites.End();

  return std::invoke(next, swapChain, syncInterval, flags);
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<NonVRD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<NonVRD3D11Kneeboard>();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-D3D11", gInstance, &ThreadEntry, hinst, dwReason, reserved);
}
