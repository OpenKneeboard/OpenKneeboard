// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IUILayer.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <memory>

namespace OpenKneeboard {
class D2DErrorRenderer;
struct DXResources;

class TabViewUILayer final : public IUILayer {
 public:
  TabViewUILayer(const audited_ptr<DXResources>& dxr);
  virtual ~TabViewUILayer();

  std::optional<D2D1_POINT_2F> GetCursorPoint() const;

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

 private:
  void RenderError(
    ID2D1DeviceContext*,
    std::string_view text,
    const PixelRect& rect);

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  std::optional<D2D1_POINT_2F> mCursorPoint;
  winrt::com_ptr<ID2D1SolidColorBrush> mErrorBackgroundBrush;
};

}// namespace OpenKneeboard
