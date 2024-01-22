// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

Texture2D<float4> Textures[] : register(t0);
sampler TextureSamplers[] : register(s0);

void SpriteVertexShader(
    inout uint textureIndex : TEXTURE_INDEX,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout float4 position   : SV_Position)
{
    // Nothing to do, no transformation matrices here.
}

float4 SpritePixelShader_impl(
    Texture2D<float4> texture,
    sampler textureSampler,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0) : SV_Target0
{
    return texture.Sample(textureSampler, texCoord) * color;
}


float4 SpritePixelShader(
    uint textureIndex : TEXTURE_INDEX,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0) : SV_Target0
{
    return SpritePixelShader_impl(
        Textures[NonUniformResourceIndex(textureIndex)],
        TextureSamplers[NonUniformResourceIndex(textureIndex)],
        color,
        texCoord);
}