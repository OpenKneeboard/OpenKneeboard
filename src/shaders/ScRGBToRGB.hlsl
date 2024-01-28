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

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);

float4 ScRGBToRGB(
    float4 color : COLOR0,
    float2 texCoord : TEXCOORD0) : SV_Target0 {

    float4 scRGB = Texture.Sample(Sampler, texCoord);
    float4 linearRGB = saturate((scRGB - 4096) / 8192);

    return linearRGB * color;        
}