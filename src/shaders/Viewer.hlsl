// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

cbuffer DrawInfo {
    float2 dimensions; 
};

struct Fill {
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    uint colorStride : COLORSTRIDE;
};

void ViewerVertexShader(
    inout float4 position: SV_Position,
    inout Fill fill,
    out float2 pixelPosition: PXPOS) {

    pixelPosition = position.xy;
    position.xy = (2 * (position.xy / dimensions)) - 1;
    position.zw = position.zw;
    fill = fill;
 };

 float4 ViewerPixelShader(
    float4 position: SV_Position,
    Fill fill,
    float2 pixelPosition: PXPOS): SV_Target0 {
    uint2 colorCoord = pixelPosition / fill.colorStride;
    if (((colorCoord.x + colorCoord.y) % 2) == 0) {
        return fill.color0;
    }
    return fill.color1;
 };