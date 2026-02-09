// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/D3D11.hpp>

#include <shims/winrt/base.h>

#include <windows.data.pdf.interop.h>

#include <source_location>
#include <stdexcept>

#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_6.h>
#include <wincodec.h>

namespace OpenKneeboard {

/** Resources needed for anything in OpenKneeboard using D3D11.
 *
 * This includes:
 * - the main app
 * - the SteamVR implementation (which uses its' own devices)
 * - the viewer
 */
struct D3D11Resources {
  class UnusableError : public std::runtime_error {
   public:
    explicit UnusableError(const HRESULT hr);
    HRESULT getHRESULT() const noexcept { return mHR; }

   private:
    HRESULT mHR;
  };
  D3D11Resources();
  ~D3D11Resources();
  D3D11Resources(const D3D11Resources&) = delete;
  D3D11Resources& operator=(const D3D11Resources&) = delete;

  winrt::com_ptr<IDXGIFactory6> mDXGIFactory;
  winrt::com_ptr<IDXGIAdapter4> mDXGIAdapter;
  uint64_t mAdapterLUID;

  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  winrt::com_ptr<IDXGIDevice2> mDXGIDevice;

  // Use `std::unique_lock`
  void lock();
  bool try_lock();
  void unlock();

 private:
  struct Locks;
  std::unique_ptr<Locks> mLocks;
};

/** Additional resources needed for Direct2D + DirectWrite.
 *
 * I've included DirectWrite here for now as it's the only current
 * reason for using Direct2D.
 *
 * D3D should be preferred in new code for basic primitives.
 */
struct D2DResources {
  D2DResources() = delete;
  D2DResources(D3D11Resources*);
  ~D2DResources();
  D2DResources(const D2DResources&) = delete;
  D2DResources& operator=(const D2DResources&) = delete;

  winrt::com_ptr<ID2D1Factory1> mD2DFactory;

  winrt::com_ptr<ID2D1Device> mD2DDevice;
  winrt::com_ptr<ID2D1DeviceContext5> mD2DDeviceContext;

  winrt::com_ptr<IDWriteFactory> mDWriteFactory;

  // Use like push/pop, but only one is allowed at a time; this exists
  // to get better debugging information/breakpoints when that's not the case
  void PushD2DDraw(std::source_location = std::source_location::current());
  HRESULT PopD2DDraw();

 private:
  struct Locks;
  std::unique_ptr<Locks> mLocks;
};

/// Resources for the OpenKneeboard app
struct DXResources : public D3D11Resources, public D2DResources {
  DXResources();
  DXResources(const DXResources&) = delete;
  DXResources(DXResources&&) = delete;
  DXResources& operator=(const DXResources&) = delete;
  DXResources& operator=(DXResources&&) = delete;

  std::unique_ptr<D3D11::SpriteBatch> mSpriteBatch;

  // e.g. doodles draw to a separate texture
  winrt::com_ptr<ID2D1DeviceContext5> mD2DBackBufferDeviceContext;

  winrt::com_ptr<IWICImagingFactory> mWIC;

  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;

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
};

}// namespace OpenKneeboard
