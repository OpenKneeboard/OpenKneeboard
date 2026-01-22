// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "UIDataItem.g.h"
// clang-format on

using namespace winrt::Windows::Foundation;

namespace winrt::OpenKneeboardApp::implementation {
struct UIDataItem : UIDataItemT<UIDataItem> {
  UIDataItem() = default;

  inline UIDataItem(const hstring& label) { Label(label); }

  hstring Label();
  void Label(hstring const& value);

  inline hstring ToString() { return Label(); }

  IInspectable Tag();
  void Tag(const IInspectable& value);

 private:
  winrt::hstring mLabel;
  IInspectable mTag;
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct UIDataItem : UIDataItemT<UIDataItem, implementation::UIDataItem> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
