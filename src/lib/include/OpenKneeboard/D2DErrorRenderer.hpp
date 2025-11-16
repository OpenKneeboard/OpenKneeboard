// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <memory>

#include <d2d1_1.h>

namespace OpenKneeboard {

struct DXResources;

class D2DErrorRenderer final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  D2DErrorRenderer(const audited_ptr<DXResources>&);
  D2DErrorRenderer(IDWriteFactory*, ID2D1SolidColorBrush*);
  D2DErrorRenderer() = delete;
  ~D2DErrorRenderer();

  void Render(
    ID2D1DeviceContext*,
    std::string_view text,
    const D2D1_RECT_F& where,
    ID2D1Brush* brush = nullptr);
};

}// namespace OpenKneeboard
