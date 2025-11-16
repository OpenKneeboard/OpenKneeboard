// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IUILayer.hpp>

namespace OpenKneeboard {

class UILayerBase : public virtual IUILayer {
 public:
  virtual ~UILayerBase();

 protected:
  void PostNextCursorEvent(
    const IUILayer::NextList& next,
    const IUILayer::Context& context,
    KneeboardViewID KneeboardViewID,
    const CursorEvent& cursorEvent);

  static constexpr inline std::tuple<IUILayer*, std::span<IUILayer*>> Split(
    const IUILayer::NextList& next) {
    return {next.front(), next.subspan(1)};
  }
};

}// namespace OpenKneeboard
