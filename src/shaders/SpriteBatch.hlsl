// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

// Modified for OpenKneeboard::Vulkan::SpriteBatch:
// - use descriptor indexing
// - take pixel coordinates and convert to normalized coordinates
// - add VK annotations
//
// This means the shader now requires Shader Model 5.1, which despite some
// documentation to the contrary, is not supported by Direct3D 11.

// Limit MUST match C++
#define MaxSpritesPerBatch 16

#ifdef VK
#define VK_BINDING(N) [[vk::binding(N)]]
#else
#define VK_BINDING(N)
#endif

VK_BINDING(0) sampler TextureSampler : register(s0);
VK_BINDING(1) Texture2D<float4> Textures[MaxSpritesPerBatch] : register(t0);

VK_BINDING(2) cbuffer BatchData : register(b0) {
    float4 sourceClamp[MaxSpritesPerBatch];
    float4 packedSourceDimensions[MaxSpritesPerBatch / 2];
    float2 destDimensions;
};

static const float2 sourceDimensions[MaxSpritesPerBatch] = (float2[MaxSpritesPerBatch]) packedSourceDimensions;

void SpriteVertexShader(
    inout float4 position   : SV_Position,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout uint textureIndex : TEXTURE_INDEX) {

#ifdef VK
#define projection float2(1, 1);
#else
#define projection float2(1, -1);
#endif

    position.xy = ((2 * (position.xy / destDimensions)) - 1) * projection;
    position.zw = position.zw;
    color = color;
    textureIndex = textureIndex;

    texCoord = texCoord / sourceDimensions[textureIndex];
}

float4 SpritePixelShader(
    float4 position   : SV_Position,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0,
    uint textureIndex : TEXTURE_INDEX) : SV_Target0 {
    texCoord = clamp(texCoord, sourceClamp[textureIndex].xy, sourceClamp[textureIndex].zw);
    return Textures[NonUniformResourceIndex(textureIndex)].Sample(TextureSampler, texCoord) * color;
}