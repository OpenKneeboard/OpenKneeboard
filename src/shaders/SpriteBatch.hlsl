// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

Texture2D<float4> Textures[] : register(t0);
sampler TextureSampler : register(s0);


cbuffer Parameters : register(b0)
{
    row_major float4x4 MatrixTransform;
};


void SpriteVertexShader(
    inout uint textureIndex : TEXTURE_INDEX,
    inout float4 color      : COLOR0,
    inout float2 texCoord   : TEXCOORD0,
    inout float4 position   : SV_Position)
{
    position = mul(position, MatrixTransform);
}


float4 SpritePixelShader(
    uint textureIndex : TEXTURE_INDEX,
    float4 color      : COLOR0,
    float2 texCoord   : TEXCOORD0) : SV_Target0
{
    return Textures[textureIndex].Sample(TextureSampler, texCoord) * color;
}