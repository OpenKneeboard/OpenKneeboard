#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace vrperfkit {
struct D3D11State {
  ComPtr<ID3D11VertexShader> vertexShader;
  ComPtr<ID3D11PixelShader> pixelShader;
  ComPtr<ID3D11ComputeShader> computeShader;
  ComPtr<ID3D11InputLayout> inputLayout;
  D3D11_PRIMITIVE_TOPOLOGY topology;
  ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  ComPtr<ID3D11Buffer> indexBuffer;
  DXGI_FORMAT format;
  UINT offset;
  ID3D11RenderTargetView* renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  ComPtr<ID3D11DepthStencilView> depthStencil;
  ComPtr<ID3D11RasterizerState> rasterizerState;
  ComPtr<ID3D11DepthStencilState> depthStencilState;
  UINT stencilRef;
  D3D11_VIEWPORT
    viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  UINT numViewports = 0;
  ComPtr<ID3D11Buffer> vsConstantBuffer;
  ComPtr<ID3D11Buffer> psConstantBuffer;
  ComPtr<ID3D11Buffer> csConstantBuffer;
  ID3D11ShaderResourceView*
    csShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  ID3D11UnorderedAccessView* csUavs[D3D11_1_UAV_SLOT_COUNT];
};

void StoreD3D11State(ID3D11DeviceContext* context, D3D11State& state);
void RestoreD3D11State(ID3D11DeviceContext* context, const D3D11State& state);
}// namespace vrperfkit
