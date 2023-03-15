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

#include <shims/winrt/base.h>

#include <windows.data.pdf.interop.h>

#include <source_location>

#include <d2d1_2.h>
#include <d3d11_2.h>
#include <dwrite.h>
#include <dxgi1_6.h>
#include <wincodec.h>

namespace OpenKneeboard {

/** Direct2D/Direct3D/DXGI resources we want to share between multiple objects.
 *
 * For the most part, we just care that we're using the same D3D11 device, so we
 * know we can efficently copy textures to everywhere, including SHM. That just
 * means we need to share an `IDXGIDevice2`.
 *
 * I originally wanted to consider everything else 'implementation details', as
 * they're cheap one-off costs - unfortunately, there is a separate check in
 * Direct2D that any resources used together come from the same factory. So
 * share the factories too.
 */
struct DXResources {
  winrt::com_ptr<ID3D11Device2> mD3DDevice;
  winrt::com_ptr<IDXGIDevice2> mDXGIDevice;
  winrt::com_ptr<ID2D1Device> mD2DDevice;
  winrt::com_ptr<ID2D1DeviceContext> mD2DDeviceContext;
  // e.g. doodles draw to a separate texture
  winrt::com_ptr<ID2D1DeviceContext> mD2DBackBufferDeviceContext;

  winrt::com_ptr<ID2D1Factory> mD2DFactory;
  winrt::com_ptr<IDXGIFactory6> mDXGIFactory;
  winrt::com_ptr<IDWriteFactory> mDWriteFactory;

  winrt::com_ptr<IWICImagingFactory> mWIC;

  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;

  // Use like push/pop, but only one is allowed at a time; this exists
  // to get better debugging information/breakpoints when that's not the case
  void PushD2DDraw(std::source_location = std::source_location::current());
  HRESULT PopD2DDraw();

  static DXResources Create();

  // Use `std::unique_lock`
  void lock();
  bool try_lock();
  void unlock();

  // Brushes :)

  // Would like something more semantic for this one; used for:
  // - PDF background
  winrt::com_ptr<ID2D1SolidColorBrush> mWhiteBrush;
  // - PDF links
  // - Button mouseovers
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  //-  Doodle pen
  winrt::com_ptr<ID2D1SolidColorBrush> mBlackBrush;
  //-  Doodle eraser
  winrt::com_ptr<ID2D1SolidColorBrush> mEraserBrush;

  winrt::com_ptr<ID2D1SolidColorBrush> mCursorInnerBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mCursorOuterBrush;

 private:
  struct Locks;
  std::shared_ptr<Locks> mLocks;
};

}// namespace OpenKneeboard
