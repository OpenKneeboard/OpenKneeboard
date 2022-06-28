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

#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <shims/winrt.h>

#include <memory>

namespace OpenKneeboard::D3D11 {

void CopyTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  DirectX::FXMVECTOR tint);

void CopyTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  float opacity);

class RenderTargetView {
 public:
  RenderTargetView();
  virtual ~RenderTargetView();
  virtual ID3D11RenderTargetView* Get() const = 0;

  RenderTargetView(const RenderTargetView&) = delete;
  RenderTargetView(const RenderTargetView&&) = delete;
  RenderTargetView& operator=(const RenderTargetView&) = delete;
  RenderTargetView& operator=(RenderTargetView&&) = delete;
};

class RenderTargetViewFactory {
 public:
  virtual ~RenderTargetViewFactory();
  virtual std::unique_ptr<RenderTargetView> Get() const = 0;
};

class D3D11RenderTargetView final : public RenderTargetView {
 public:
  D3D11RenderTargetView() = delete;
  D3D11RenderTargetView(const winrt::com_ptr<ID3D11RenderTargetView>&);
  ~D3D11RenderTargetView();

  virtual ID3D11RenderTargetView* Get() const override;

 private:
  winrt::com_ptr<ID3D11RenderTargetView> mImpl;
};

class D3D11RenderTargetViewFactory final : public RenderTargetViewFactory {
 public:
  D3D11RenderTargetViewFactory(ID3D11Device*, ID3D11Texture2D*);
  virtual ~D3D11RenderTargetViewFactory();

  virtual std::unique_ptr<RenderTargetView> Get() const override;

 private:
  winrt::com_ptr<ID3D11RenderTargetView> mImpl;
};

class D3D11On12RenderTargetView final : public RenderTargetView {
 public:
  D3D11On12RenderTargetView() = delete;
  D3D11On12RenderTargetView(ID3D11Device*, ID3D11On12Device*, ID3D12Resource*);
  ~D3D11On12RenderTargetView();

  virtual ID3D11RenderTargetView* Get() const override;

 private:
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};

class D3D11On12RenderTargetViewFactory final : public RenderTargetViewFactory {
 public:
  D3D11On12RenderTargetViewFactory(
    const winrt::com_ptr<ID3D11Device>&,
    const winrt::com_ptr<ID3D11On12Device>&,
    const winrt::com_ptr<ID3D12Resource>& texture12);
  virtual ~D3D11On12RenderTargetViewFactory();

  virtual std::unique_ptr<RenderTargetView> Get() const override;

 private:
  winrt::com_ptr<ID3D11Device> mD3D11;
  winrt::com_ptr<ID3D11On12Device> mD3D11On12;
  winrt::com_ptr<ID3D12Resource> mTexture12;
};

}// namespace OpenKneeboard::D3D11
