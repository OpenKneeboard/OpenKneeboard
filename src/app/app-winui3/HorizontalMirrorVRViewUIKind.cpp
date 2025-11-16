// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "HorizontalMirrorVRViewUIKind.h"
#include "HorizontalMirrorVRViewUIKind.g.cpp"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
winrt::guid HorizontalMirrorVRViewUIKind::MirrorOf() {
  return mMirrorOf;
}
void HorizontalMirrorVRViewUIKind::MirrorOf(winrt::guid const& value) {
  mMirrorOf = value;
}
}// namespace winrt::OpenKneeboardApp::implementation
