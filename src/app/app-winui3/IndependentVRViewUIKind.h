// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "IndependentVRViewUIKind.g.h"
#include "UIDataItem.h"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
struct IndependentVRViewUIKind
  : IndependentVRViewUIKindT<
      IndependentVRViewUIKind,
      OpenKneeboardApp::implementation::UIDataItem> {
  IndependentVRViewUIKind();
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct IndependentVRViewUIKind : IndependentVRViewUIKindT<
                                   IndependentVRViewUIKind,
                                   implementation::IndependentVRViewUIKind> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
