// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "common.h"
#include "shader_utilities.h"

#include <OpenXR-Toolkit/d3d11.h>

namespace {

using namespace toolkit::utilities::shader;

struct D3D11ContextState {
  ComPtr<ID3D11InputLayout> inputLayout;
  D3D11_PRIMITIVE_TOPOLOGY topology;
  ComPtr<ID3D11Buffer> vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT vertexBufferStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT vertexBufferOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

  ComPtr<ID3D11Buffer> indexBuffer;
  DXGI_FORMAT indexBufferFormat;
  UINT indexBufferOffset;

  ComPtr<ID3D11RenderTargetView>
    renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  ComPtr<ID3D11DepthStencilView> depthStencil;
  ComPtr<ID3D11DepthStencilState> depthStencilState;
  UINT stencilRef;
  ComPtr<ID3D11BlendState> blendState;
  float blendFactor[4];
  UINT blendMask;

#define SHADER_STAGE_STATE(stage, programType) \
  ComPtr<programType> stage##Program; \
  ComPtr<ID3D11Buffer> \
    stage##ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]; \
  ComPtr<ID3D11SamplerState> \
    stage##Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]; \
  ComPtr<ID3D11ShaderResourceView> \
    stage##ShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

  SHADER_STAGE_STATE(VS, ID3D11VertexShader);
  SHADER_STAGE_STATE(PS, ID3D11PixelShader);
  SHADER_STAGE_STATE(GS, ID3D11GeometryShader);
  SHADER_STAGE_STATE(DS, ID3D11DomainShader);
  SHADER_STAGE_STATE(HS, ID3D11HullShader);
  SHADER_STAGE_STATE(CS, ID3D11ComputeShader);

#undef SHADER_STAGE_STATE

  ComPtr<ID3D11UnorderedAccessView>
    CSUnorderedResources[D3D11_1_UAV_SLOT_COUNT];

  ComPtr<ID3D11RasterizerState> rasterizerState;
  D3D11_VIEWPORT
  viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  UINT numViewports;
  D3D11_RECT
  scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  UINT numScissorRects;

  void save(ID3D11DeviceContext* context) {
    TraceLocalActivity(local);
    TraceLoggingWriteStart(local, "D3D11ContextState_Save");

    context->IAGetInputLayout(set(inputLayout));
    context->IAGetPrimitiveTopology(&topology);
    {
      ID3D11Buffer* vbs[ARRAYSIZE(vertexBuffers)];
      context->IAGetVertexBuffers(
        0, ARRAYSIZE(vbs), vbs, vertexBufferStrides, vertexBufferOffsets);
      for (uint32_t i = 0; i < ARRAYSIZE(vbs); i++) {
        attach(vertexBuffers[i], vbs[i]);
      }
    }
    context->IAGetIndexBuffer(
      set(indexBuffer), &indexBufferFormat, &indexBufferOffset);

    {
      ID3D11RenderTargetView* rtvs[ARRAYSIZE(renderTargets)];
      context->OMGetRenderTargets(ARRAYSIZE(rtvs), rtvs, set(depthStencil));
      for (uint32_t i = 0; i < ARRAYSIZE(rtvs); i++) {
        attach(renderTargets[i], rtvs[i]);
      }
    }

    context->OMGetDepthStencilState(set(depthStencilState), &stencilRef);
    context->OMGetBlendState(set(blendState), blendFactor, &blendMask);

#define SHADER_STAGE_SAVE_CONTEXT(stage) \
  context->stage##GetShader(set(stage##Program), nullptr, nullptr); \
  { \
    ID3D11Buffer* buffers[ARRAYSIZE(stage##ConstantBuffers)]; \
    context->stage##GetConstantBuffers(0, ARRAYSIZE(buffers), buffers); \
    for (uint32_t i = 0; i < ARRAYSIZE(buffers); i++) { \
      attach(stage##ConstantBuffers[i], buffers[i]); \
    } \
  } \
  { \
    ID3D11SamplerState* samp[ARRAYSIZE(stage##Samplers)]; \
    context->stage##GetSamplers(0, ARRAYSIZE(samp), samp); \
    for (uint32_t i = 0; i < ARRAYSIZE(samp); i++) { \
      attach(stage##Samplers[i], samp[i]); \
    } \
  } \
  { \
    ID3D11ShaderResourceView* srvs[ARRAYSIZE(stage##ShaderResources)]; \
    context->stage##GetShaderResources(0, ARRAYSIZE(srvs), srvs); \
    for (uint32_t i = 0; i < ARRAYSIZE(srvs); i++) { \
      attach(stage##ShaderResources[i], srvs[i]); \
    } \
  }

    SHADER_STAGE_SAVE_CONTEXT(VS);
    SHADER_STAGE_SAVE_CONTEXT(PS);
    SHADER_STAGE_SAVE_CONTEXT(GS);
    SHADER_STAGE_SAVE_CONTEXT(DS);
    SHADER_STAGE_SAVE_CONTEXT(HS);
    SHADER_STAGE_SAVE_CONTEXT(CS);

#undef SHADER_STAGE_SAVE_CONTEXT

    {
      ID3D11UnorderedAccessView* uavs[ARRAYSIZE(CSUnorderedResources)];
      context->CSGetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs);
      for (uint32_t i = 0; i < ARRAYSIZE(uavs); i++) {
        attach(CSUnorderedResources[i], uavs[i]);
      }
    }

    context->RSGetState(set(rasterizerState));
    numViewports = ARRAYSIZE(viewports);
    context->RSGetViewports(&numViewports, viewports);
    numScissorRects = ARRAYSIZE(scissorRects);
    context->RSGetScissorRects(&numScissorRects, scissorRects);

    m_isValid = true;

    TraceLoggingWriteStop(local, "D3D11ContextState_Save");
  }

  void restore(ID3D11DeviceContext* context) const {
    TraceLocalActivity(local);
    TraceLoggingWriteStart(local, "D3D11ContextState_Restore");

    context->IASetInputLayout(get(inputLayout));
    context->IASetPrimitiveTopology(topology);
    {
      ID3D11Buffer* vbs[ARRAYSIZE(vertexBuffers)];
      for (uint32_t i = 0; i < ARRAYSIZE(vbs); i++) {
        vbs[i] = get(vertexBuffers[i]);
      }
      context->IASetVertexBuffers(
        0, ARRAYSIZE(vbs), vbs, vertexBufferStrides, vertexBufferOffsets);
    }
    context->IASetIndexBuffer(
      get(indexBuffer), indexBufferFormat, indexBufferOffset);

    {
      ID3D11RenderTargetView* rtvs[ARRAYSIZE(renderTargets)];
      for (uint32_t i = 0; i < ARRAYSIZE(rtvs); i++) {
        rtvs[i] = get(renderTargets[i]);
      }
      context->OMSetRenderTargets(ARRAYSIZE(rtvs), rtvs, get(depthStencil));
    }
    context->OMSetDepthStencilState(get(depthStencilState), stencilRef);
    context->OMSetBlendState(get(blendState), blendFactor, blendMask);

#define SHADER_STAGE_RESTORE_CONTEXT(stage) \
  context->stage##SetShader(get(stage##Program), nullptr, 0); \
  { \
    ID3D11Buffer* buffers[ARRAYSIZE(stage##ConstantBuffers)]; \
    for (uint32_t i = 0; i < ARRAYSIZE(buffers); i++) { \
      buffers[i] = get(stage##ConstantBuffers[i]); \
    } \
    context->stage##SetConstantBuffers(0, ARRAYSIZE(buffers), buffers); \
  } \
  { \
    ID3D11SamplerState* samp[ARRAYSIZE(stage##Samplers)]; \
    for (uint32_t i = 0; i < ARRAYSIZE(samp); i++) { \
      samp[i] = get(stage##Samplers[i]); \
    } \
    context->stage##SetSamplers(0, ARRAYSIZE(samp), samp); \
  } \
  { \
    ID3D11ShaderResourceView* srvs[ARRAYSIZE(stage##ShaderResources)]; \
    for (uint32_t i = 0; i < ARRAYSIZE(srvs); i++) { \
      srvs[i] = get(stage##ShaderResources[i]); \
    } \
    context->stage##SetShaderResources(0, ARRAYSIZE(srvs), srvs); \
  }

    SHADER_STAGE_RESTORE_CONTEXT(VS);
    SHADER_STAGE_RESTORE_CONTEXT(PS);
    SHADER_STAGE_RESTORE_CONTEXT(GS);
    SHADER_STAGE_RESTORE_CONTEXT(DS);
    SHADER_STAGE_RESTORE_CONTEXT(HS);
    SHADER_STAGE_RESTORE_CONTEXT(CS);

#undef SHADER_STAGE_RESTORE_CONTEXT

    {
      ID3D11UnorderedAccessView* uavs[ARRAYSIZE(CSUnorderedResources)];
      for (uint32_t i = 0; i < ARRAYSIZE(uavs); i++) {
        uavs[i] = get(CSUnorderedResources[i]);
      }
      context->CSGetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs);
    }

    context->RSSetState(get(rasterizerState));
    context->RSSetViewports(numViewports, viewports);
    context->RSSetScissorRects(numScissorRects, scissorRects);

    TraceLoggingWriteStop(local, "D3D11ContextState_Restore");
  }

  void clear() {
#define RESET_ARRAY(array) \
  for (uint32_t i = 0; i < ARRAYSIZE(array); i++) { \
    array[i].Reset(); \
  }

    inputLayout.Reset();
    RESET_ARRAY(vertexBuffers);
    indexBuffer.Reset();

    RESET_ARRAY(renderTargets);
    depthStencil.Reset();
    depthStencilState.Reset();
    blendState.Reset();

#define SHADER_STAGE_STATE(stage) \
  stage##Program.Reset(); \
  RESET_ARRAY(stage##ConstantBuffers); \
  RESET_ARRAY(stage##Samplers); \
  RESET_ARRAY(stage##ShaderResources);

    SHADER_STAGE_STATE(VS);
    SHADER_STAGE_STATE(PS);
    SHADER_STAGE_STATE(GS);
    SHADER_STAGE_STATE(DS);
    SHADER_STAGE_STATE(HS);
    SHADER_STAGE_STATE(CS);

    RESET_ARRAY(CSUnorderedResources);

    rasterizerState.Reset();

#undef RESET_ARRAY

    m_isValid = false;
  }

  bool isValid() const {
    return m_isValid;
  }

 private:
  bool m_isValid {false};
};

}// namespace

namespace OpenXRToolkit::D3D11 {

struct SavedState::Impl {
  ComPtr<ID3D11DeviceContext> m_context;
  D3D11ContextState m_state {};
};

SavedState::SavedState(ID3D11DeviceContext* ctx) {
  m_impl = new Impl({.m_context = ctx});
  m_impl->m_state.save(ctx);
}

SavedState::~SavedState() {
  m_impl->m_state.restore(m_impl->m_context.Get());
}

}// namespace OpenXRToolkit::D3D11