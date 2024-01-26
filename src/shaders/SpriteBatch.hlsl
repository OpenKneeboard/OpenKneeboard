// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

// Limit MUST match C++
#define MaxSpritesPerBatch 16
[[vk::binding(0)]] sampler TextureSampler : register(s0);
[[vk::binding(1)]] Texture2D<float4> Textures[MaxSpritesPerBatch] : register(t0);

struct BatchData {
    float2 destDimensions;
    float2 sourceDimensions[MaxSpritesPerBatch];
};

[[vk::push_constant]]
BatchData batchData;

void SpriteVertexShader(
    inout float4 position   : SV_Position,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout uint textureIndex : TEXTURE_INDEX)
{
    position.xy = (2 * (position.xy / batchData.destDimensions)) - 1;
    position.zw = position.zw;
    color = color;
    texCoord = texCoord / batchData.sourceDimensions[textureIndex];
    textureIndex = textureIndex;
}

float4 SpritePixelShader(
    float4 position   : SV_Position,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0,
    uint textureIndex : TEXTURE_INDEX) : SV_Target0
{
    return Textures[NonUniformResourceIndex(textureIndex)].Sample(TextureSampler, texCoord) * color;
}