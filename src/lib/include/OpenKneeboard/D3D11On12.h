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
#pragma once

#include <OpenKneeboard/D3D11.h>
#include <d3d11on12.h>

namespace OpenKneeboard::D3D11On12 {

struct DeviceResources {
  winrt::com_ptr<ID3D11Device> mDevice11;
  winrt::com_ptr<ID3D11DeviceContext> mContext11;

  winrt::com_ptr<ID3D11On12Device2> m11on12;

  winrt::com_ptr<ID3D12Device> mDevice12;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue12;
};

enum class Flags : uint16_t {
  None = 0,
  DoubleBuffer = 1,// For Varjo
};

class D3D11On12RenderTargetView final : public D3D11::IRenderTargetView {
 public:
  D3D11On12RenderTargetView() = delete;
  D3D11On12RenderTargetView(
    const DeviceResources&,
    const winrt::com_ptr<ID3D12Resource>& texture12,
    const winrt::com_ptr<ID3D11Texture2D>& texture11,
    const winrt::com_ptr<ID3D11Texture2D>& bufferTexture11,
    const winrt::com_ptr<ID3D11RenderTargetView>&);
  ~D3D11On12RenderTargetView();

  virtual ID3D11RenderTargetView* Get() const override;

 private:
  DeviceResources mDeviceResources;
  winrt::com_ptr<ID3D12Resource> mTexture12;
  winrt::com_ptr<ID3D11Texture2D> mTexture11;
  winrt::com_ptr<ID3D11Texture2D> mBufferTexture11;
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};

class D3D11On12RenderTargetViewFactory final
  : public D3D11::IRenderTargetViewFactory {
 public:
  D3D11On12RenderTargetViewFactory(
    const DeviceResources&,
    const winrt::com_ptr<ID3D12Resource>& texture12,
    Flags flags = Flags::None);
  virtual ~D3D11On12RenderTargetViewFactory();

  virtual std::unique_ptr<D3D11::IRenderTargetView> Get() const override;

 private:
  DeviceResources mDeviceResources;
  winrt::com_ptr<ID3D12Resource> mTexture12;
  winrt::com_ptr<ID3D11Texture2D> mTexture11;
  winrt::com_ptr<ID3D11Texture2D> mBufferTexture11;
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};

}// namespace OpenKneeboard::D3D11On12

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<D3D11On12::Flags> = true;
}
