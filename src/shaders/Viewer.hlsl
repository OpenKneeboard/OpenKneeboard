/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

cbuffer DrawInfo {
    float2 dimensions; 
};

struct Fill {
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
};

void ViewerVertexShader(
    inout float4 position: SV_Position,
    inout Fill fill) {

    position.xy = (2 * (position.xy / dimensions)) - 1;
    position.zw = position.zw;
    fill = fill;
 };

 float4 ViewerPixelShader(
    float4 position: SV_Position,
    Fill fill): SV_Target0 {
    uint idx = (position.x > 0) + (position.y > 0);
    return (idx % 2) ? fill.color0 : fill.color1;
 };