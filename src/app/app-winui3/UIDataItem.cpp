// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "UIDataItem.h"
#include "UIDataItem.g.cpp"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
hstring UIDataItem::Label() {
  return mLabel;
}

void UIDataItem::Label(hstring const& value) {
  mLabel = value;
}

winrt::Windows::Foundation::IInspectable UIDataItem::Tag() {
  return mTag;
}

void UIDataItem::Tag(const IInspectable& data) {
  mTag = data;
}

}// namespace winrt::OpenKneeboardApp::implementation
