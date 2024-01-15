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

#include <OpenKneeboard/SHM.h>

namespace OpenKneeboard::SHM::D3D11 {

class LayerTextureCache : public SHM::LayerTextureCache {
 public:
  using SHM::LayerTextureCache::LayerTextureCache;

  virtual ~LayerTextureCache();
  ID3D11ShaderResourceView* GetD3D11ShaderResourceView();

 private:
  winrt::com_ptr<ID3D11ShaderResourceView> mD3D11ShaderResourceView;
};

class CachedReader : public SHM::CachedReader {
 public:
  using SHM::CachedReader::CachedReader;

 protected:
  virtual std::shared_ptr<SHM::LayerTextureCache> CreateLayerTextureCache(
    const winrt::com_ptr<ID3D11Texture2D>&) override;
};

};// namespace OpenKneeboard::SHM::D3D11