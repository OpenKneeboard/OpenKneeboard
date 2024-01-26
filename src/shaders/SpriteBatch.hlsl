// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

// Limit of 16 textures should match C++
[[vk::binding(0)]] sampler TextureSampler : register(s0);
//[[vk::binding(1)]] Texture2D<float4> Textures[] : register(t0);
[[vk::binding(1)]] Texture2D<float4> Textures : register(t0);

struct BatchData {
    float2 destDimensions;
};

[[vk::push_constant]]
BatchData batchData;

void SpriteVertexShader(
    inout float4 position   : SV_Position,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout uint textureIndex : TEXTURE_INDEX)
{
    position = position;
    position[0] = (2 * (position[0] / batchData.destDimensions[0])) - 1;
    position[1] = (2 * (position[1] / batchData.destDimensions[1])) - 1;
    color = color;
    texCoord = texCoord;
    textureIndex = textureIndex;
}

float4 SpritePixelShader(
    float4 position   : SV_Position,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0,
    uint textureIndex : TEXTURE_INDEX) : SV_Target0
{
    //return Textures[NonUniformResourceIndex(textureIndex)].Sample(TextureSampler, texCoord) * color;
    return Textures.Sample(TextureSampler, texCoord) * color;
}