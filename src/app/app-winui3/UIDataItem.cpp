/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
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

IInspectable UIDataItem::Tag() {
  return mTag;
}

void UIDataItem::Tag(const IInspectable& data) {
  mTag = data;
}

}// namespace winrt::OpenKneeboardApp::implementation
