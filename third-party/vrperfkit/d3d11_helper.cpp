#include <vrperfkit/d3d11_helper.h>

namespace vrperfkit {

void StoreD3D11State(ID3D11DeviceContext* context, D3D11State& state) {
  context->VSGetShader(
    state.vertexShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
  context->PSGetShader(
    state.pixelShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
  context->CSGetShader(
    state.computeShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
  context->IAGetInputLayout(state.inputLayout.ReleaseAndGetAddressOf());
  context->IAGetPrimitiveTopology(&state.topology);
  context->IAGetVertexBuffers(
    0,
    D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
    state.vertexBuffers,
    state.strides,
    state.offsets);
  context->IAGetIndexBuffer(
    state.indexBuffer.ReleaseAndGetAddressOf(), &state.format, &state.offset);
  context->OMGetRenderTargets(
    D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
    state.renderTargets,
    state.depthStencil.GetAddressOf());
  context->RSGetState(state.rasterizerState.ReleaseAndGetAddressOf());
  context->OMGetDepthStencilState(
    state.depthStencilState.ReleaseAndGetAddressOf(), &state.stencilRef);
  context->RSGetViewports(&state.numViewports, nullptr);
  context->RSGetViewports(&state.numViewports, state.viewports);
  context->VSGetConstantBuffers(0, 1, state.vsConstantBuffer.GetAddressOf());
  context->PSGetConstantBuffers(0, 1, state.psConstantBuffer.GetAddressOf());
  context->CSGetConstantBuffers(0, 1, state.csConstantBuffer.GetAddressOf());
  context->CSGetShaderResources(
    0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.csShaderResources);
  context->CSGetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, state.csUavs);
}

void RestoreD3D11State(ID3D11DeviceContext* context, const D3D11State& state) {
  context->VSSetShader(state.vertexShader.Get(), nullptr, 0);
  context->PSSetShader(state.pixelShader.Get(), nullptr, 0);
  context->CSSetShader(state.computeShader.Get(), nullptr, 0);
  context->IASetInputLayout(state.inputLayout.Get());
  context->IASetPrimitiveTopology(state.topology);
  context->IASetVertexBuffers(
    0,
    D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
    state.vertexBuffers,
    state.strides,
    state.offsets);
  for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i) {
    if (state.vertexBuffers[i])
      state.vertexBuffers[i]->Release();
  }
  context->IASetIndexBuffer(
    state.indexBuffer.Get(), state.format, state.offset);
  context->OMSetRenderTargets(
    D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
    state.renderTargets,
    state.depthStencil.Get());
  for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
    if (state.renderTargets[i]) {
      state.renderTargets[i]->Release();
    }
  }
  context->RSSetState(state.rasterizerState.Get());
  context->OMSetDepthStencilState(
    state.depthStencilState.Get(), state.stencilRef);
  context->RSSetViewports(state.numViewports, state.viewports);
  context->VSSetConstantBuffers(0, 1, state.vsConstantBuffer.GetAddressOf());
  context->PSSetConstantBuffers(0, 1, state.psConstantBuffer.GetAddressOf());
  context->CSSetConstantBuffers(0, 1, state.csConstantBuffer.GetAddressOf());
  context->CSSetShaderResources(
    0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.csShaderResources);
  for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i) {
    if (state.csShaderResources[i])
      state.csShaderResources[i]->Release();
  }
  UINT initial = 0;
  context->CSSetUnorderedAccessViews(
    0, D3D11_1_UAV_SLOT_COUNT, state.csUavs, &initial);
  for (int i = 0; i < D3D11_1_UAV_SLOT_COUNT; ++i) {
    if (state.csUavs[i])
      state.csUavs[i]->Release();
  }
}
}// namespace vrperfkit
