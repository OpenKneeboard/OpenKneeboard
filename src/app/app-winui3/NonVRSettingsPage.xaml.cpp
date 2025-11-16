// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "NonVRSettingsPage.xaml.h"
#include "NonVRSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/utf8.hpp>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

NonVRSettingsPage::NonVRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
}

OpenKneeboard::fire_and_forget NonVRSettingsPage::RestoreDefaults(
  IInspectable,
  RoutedEventArgs) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default non-VR "
                           "settings, removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  SetViewConfig({
    .mEnabled = true,
  });

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

uint8_t NonVRSettingsPage::KneeboardHeightPercent() {
  return this->GetViewConfig().mConstraints.mHeightPercent;
}

void NonVRSettingsPage::KneeboardHeightPercent(uint8_t value) {
  auto config = this->GetViewConfig();
  config.mConstraints.mHeightPercent = value;
  this->SetViewConfig(config);
}

uint32_t NonVRSettingsPage::KneeboardPaddingPixels() {
  return this->GetViewConfig().mConstraints.mPaddingPixels;
}

void NonVRSettingsPage::KneeboardPaddingPixels(uint32_t value) {
  auto config = this->GetViewConfig();
  config.mConstraints.mPaddingPixels = value;
  this->SetViewConfig(config);
}

float NonVRSettingsPage::KneeboardOpacity() {
  return this->GetViewConfig().mOpacity * 100;
}

void NonVRSettingsPage::KneeboardOpacity(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = this->GetViewConfig();
  config.mOpacity = value / 100;
  this->SetViewConfig(config);
}

uint8_t NonVRSettingsPage::KneeboardHorizontalPlacement() {
  return static_cast<uint8_t>(
    this->GetViewConfig().mConstraints.mHorizontalAlignment);
}

void NonVRSettingsPage::KneeboardHorizontalPlacement(uint8_t value) {
  auto config = this->GetViewConfig();
  config.mConstraints.mHorizontalAlignment
    = static_cast<Alignment::Horizontal>(value);
  this->SetViewConfig(config);
}

uint8_t NonVRSettingsPage::KneeboardVerticalPlacement() {
  return static_cast<uint8_t>(
    this->GetViewConfig().mConstraints.mVerticalAlignment);
}

void NonVRSettingsPage::KneeboardVerticalPlacement(uint8_t value) {
  auto config = this->GetViewConfig();
  config.mConstraints.mVerticalAlignment
    = static_cast<Alignment::Vertical>(value);
  this->SetViewConfig(config);
}

ViewNonVRSettings NonVRSettingsPage::GetViewConfig() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  if (mCurrentView >= views.size()) [[unlikely]] {
    dprint("View {} >= count {}", mCurrentView, views.size());
    OPENKNEEBOARD_BREAK;
    // TODO: uncomment when > 1 non-VR view is supported.
    // mCurrentView = 0;
    return {};
  }
  return views.at(mCurrentView).mNonVR;
}

OpenKneeboard::fire_and_forget NonVRSettingsPage::SetViewConfig(
  ViewNonVRSettings value) {
  auto viewsConfig = mKneeboard->GetViewsSettings();
  auto& views = viewsConfig.mViews;
  if (mCurrentView >= views.size()) [[unlikely]] {
    dprint("View {} >= count {}", mCurrentView, views.size());
    OPENKNEEBOARD_BREAK;
    co_return;
  }
  views[mCurrentView].mNonVR = value;
  co_await mKneeboard->SetViewsSettings(viewsConfig);
}

}// namespace winrt::OpenKneeboardApp::implementation
