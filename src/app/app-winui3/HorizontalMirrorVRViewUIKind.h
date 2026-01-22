// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "HorizontalMirrorVRViewUIKind.g.h"
#include "UIDataItem.h"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
struct HorizontalMirrorVRViewUIKind
  : HorizontalMirrorVRViewUIKindT<
      HorizontalMirrorVRViewUIKind,
      OpenKneeboardApp::implementation::UIDataItem> {
  HorizontalMirrorVRViewUIKind() = default;

  winrt::guid MirrorOf();
  void MirrorOf(winrt::guid const& value);

 private:
  winrt::guid mMirrorOf;
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct HorizontalMirrorVRViewUIKind
  : HorizontalMirrorVRViewUIKindT<
      HorizontalMirrorVRViewUIKind,
      implementation::HorizontalMirrorVRViewUIKind> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
