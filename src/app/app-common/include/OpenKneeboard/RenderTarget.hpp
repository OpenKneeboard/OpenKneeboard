// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/RenderTargetID.hpp>
#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <Windows.h>
#include <d3d11.h>

#include <memory>

#include <d2d1_3.h>

namespace OpenKneeboard {

struct DXResources;

/** Encapsulate a render target for use with either D3D11 or D2D.
 *
 * Only D2D or D3D can be active, and if acquired, the objects
 * must be released before passing the RenderTarget* on, or the
 * callee will not be able to use it.
 */
class RenderTarget : public std::enable_shared_from_this<RenderTarget> {
 public:
  enum class State {
    Unattached,
    D2D,
    D3D,
  };

  RenderTarget() = delete;
  virtual ~RenderTarget();

  static std::shared_ptr<RenderTarget> Create(
    const audited_ptr<DXResources>& dxr,
    const winrt::com_ptr<ID3D11Texture2D>& texture);
  static std::shared_ptr<RenderTarget> Create(
    const audited_ptr<DXResources>& dxr,
    nullptr_t texture);

  PixelSize GetDimensions() const;
  virtual RenderTargetID GetID() const;

  void SetD3DTexture(const winrt::com_ptr<ID3D11Texture2D>&);

  class D2D;
  class D3D;
  friend class D2D;
  friend class D3D;

  D2D d2d(const std::source_location& loc = std::source_location::current());
  D3D d3d(const std::source_location& loc = std::source_location::current());

 protected:
  RenderTarget(
    const audited_ptr<DXResources>& dxr,
    const winrt::com_ptr<ID3D11Texture2D>& texture);

 private:
  StateMachine<
    State,
    State::Unattached,
    std::array {
      Transition {State::Unattached, State::D2D},
      Transition {State::Unattached, State::D3D},
      Transition {State::D2D, State::Unattached},
      Transition {State::D3D, State::Unattached},
    }>
    mState;

  PixelSize mDimensions;

  audited_ptr<DXResources> mDXR;

  RenderTargetID mID;

  winrt::com_ptr<ID2D1Bitmap1> mD2DBitmap;

  winrt::com_ptr<ID3D11Texture2D> mD3DTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mD3DRenderTargetView;
};

/** RenderTarget with multiple identities for cache management purposes.
 *
 * For example, the interprocess render uses a single canvas, so a single render
 * target; however, RTID is used for cache indexing, so every active view should
 * have a distinct ID.
 *
 * This effectively allows multiple RTIDs to share a single set of D3D
 * resources.
 */
class RenderTargetWithMultipleIdentities : public RenderTarget {
 public:
  static std::shared_ptr<RenderTargetWithMultipleIdentities> Create(
    const audited_ptr<DXResources>& dxr,
    const winrt::com_ptr<ID3D11Texture2D>& texture,
    size_t identityCount);

  virtual RenderTargetID GetID() const override;
  void SetActiveIdentity(size_t index);

 protected:
  using RenderTarget::RenderTarget;

 private:
  std::vector<RenderTargetID> mIdentities;
  size_t mCurrentIdentity {0};
};

class RenderTarget::D2D final {
 public:
  D2D() = delete;
  D2D(const D2D&) = delete;
  D2D(D2D&&) noexcept;
  D2D& operator=(const D2D&) = delete;
  D2D& operator=(D2D&&) = delete;

  D2D(const std::shared_ptr<RenderTarget>&, const std::source_location&);
  ~D2D();

  ID2D1DeviceContext* operator->() const;
  operator ID2D1DeviceContext*() const;

  void Release();
  void Reacquire();

 private:
  bool mReleased {false};
  std::shared_ptr<RenderTarget> mParent;
  std::source_location mSourceLocation;
  RenderTarget* mUnsafeParent {nullptr};
  bool mHDR {false};

  void Acquire();
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    mTraceLoggingActivity,
    "RenderTarget::D2D");
};

class RenderTarget::D3D final {
 public:
  D3D() = delete;
  D3D(const D3D&) = delete;
  D3D(D3D&&) = delete;
  D3D& operator=(const D3D&) = delete;
  D3D& operator=(D3D&&) = delete;

  D3D(const std::shared_ptr<RenderTarget>&);
  ~D3D();

  ID3D11Texture2D* texture() const;
  ID3D11RenderTargetView* rtv() const;

 private:
  std::shared_ptr<RenderTarget> mParent;
  RenderTarget* mUnsafeParent {nullptr};

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    mTraceLoggingActivity,
    "RenderTarget::D3D");
};

class KneeboardView;

class RenderContext {
 public:
  RenderContext() = delete;
  constexpr RenderContext(RenderTarget* rt, KneeboardView* view)
    : mRenderTarget(rt),
      mKneeboardView(view) {}

  inline auto d2d(
    const std::source_location& caller =
      std::source_location::current()) const noexcept {
    return mRenderTarget->d2d(caller);
  }

  inline auto d3d(
    const std::source_location& caller =
      std::source_location::current()) const noexcept {
    return mRenderTarget->d3d(caller);
  }

  constexpr auto GetRenderTarget() const noexcept { return mRenderTarget; }

  /// WARNING: may be nullptr for renders that aren't tied to a particular view,
  /// e.g. caches or navigation preview
  constexpr auto GetKneeboardView() const noexcept { return mKneeboardView; }

 private:
  // Represents the GPU texture that is being rendered to
  RenderTarget* mRenderTarget {nullptr};

  // Mostly for debugging, e.g. for page-based web dashboards, this shows up in
  // the logs to explain the multiple instances
  KneeboardView* mKneeboardView {nullptr};
};

}// namespace OpenKneeboard
