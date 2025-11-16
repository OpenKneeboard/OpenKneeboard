// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

// Simplified version of SpriteBatch.hlsl to only support one sprite at a time,
// so that descriptor indexing is not required.
// This means that Shader Model 5.0 is supported, so D3D11 is supported.

// Limit MUST match C++
sampler TextureSampler : register(s0);
Texture2D<float4> Texture: register(t0);

cbuffer UniformData: register(b0) {
    float2 sourceDimensions;
    float2 destDimensions;
};

void SpriteVertexShader(
    inout float4 position   : SV_Position,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout float2 clampTL    : TEXCOORD1,
    inout float2 clampBR    : TEXCOORD2) {

    // Y axis is up in D3D, and that's all we care about for this shader; Vulkan
    // uses SpriteBatch instead.
    position.y = destDimensions.y - position.y;
    position.xy = (2 * (position.xy / destDimensions)) - 1;
    position.zw = position.zw;
    color = color;

    texCoord = texCoord / sourceDimensions;
    clampTL = clampTL;
    clampBR = clampBR;
}

float4 SpritePixelShader(
    float4 position   : SV_Position,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0,
    float2 clampTL    : TEXCOORD1,
    float2 clampBR    : TEXCOORD2): SV_Target0 {
    texCoord = clamp(texCoord, clampTL, clampBR);
    return Texture.Sample(TextureSampler, texCoord) * color;
}