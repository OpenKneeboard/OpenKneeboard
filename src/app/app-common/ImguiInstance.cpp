#include "OpenKneeboard/scope_guard.h"

#include <OpenKneeboard/ImguiInstance.h>

#include <vrperfkit/d3d11_helper.h>

#include <imgui_impl_dx11.h>

namespace OpenKneeboard {
ImguiInstance::ImguiInstance(const DXResources& dxr, KneeboardState* kbs)
  : mDXR(dxr), mKneeboard(kbs) {
  m2DContext = mDXR.mD2DBackBufferDeviceContext;

  auto device = dxr.mD3DDevice.get();
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());

  // init IMGUI
  mImguiCtx = ImGui::CreateContext();
  ImGui::SetCurrentContext(mImguiCtx);
  ImGui_ImplDX11_Init((ID3D11Device*)device, ctx.get());

  // Create texture buffer
  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = 768,
    .Height = 1024,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
  };

  device->CreateTexture2D(&textureDesc, nullptr, mTexture.put());

  mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    mTexture.as<IDXGISurface>().get(), nullptr, mBitmap.put());

  // we're gonna change render targets and generally screw with the state to
  // render out IMGUI, these helpers should help us restore the state afterwards
  vrperfkit::D3D11State state {};
  vrperfkit::StoreD3D11State(ctx.get(), state);
  scope_guard restoreState(
    [&]() { vrperfkit::RestoreD3D11State(ctx.get(), state); });

  ID3D11RenderTargetView* backbuffer;// global declaration
  device->CreateRenderTargetView(mTexture.get(), nullptr, &backbuffer);
  ctx->OMSetRenderTargets(1, &backbuffer, nullptr);

  D3D11_VIEWPORT viewport;
  ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = 768;
  viewport.Height = 1024;

  ctx->RSSetViewports(1, &viewport);

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(768, 1024);
  ImGui_ImplDX11_NewFrame();

  ImGui::NewFrame();
  ImGui::Text("Hello, world!");

  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

ImguiInstance::~ImguiInstance() {
  ImGui::SetCurrentContext(mImguiCtx);
  ImGui_ImplDX11_Shutdown();
  ImGui::DestroyContext(mImguiCtx);
}

void ImguiInstance::Render(
  ID2D1DeviceContext* ctx,
  PageID,
  const D2D1_RECT_F& targetRect) {
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawBitmap(
    mBitmap.get(),
    targetRect,
    1.0f,
    D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}
}// namespace OpenKneeboard