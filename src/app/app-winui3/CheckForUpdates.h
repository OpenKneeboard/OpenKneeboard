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
#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <shims/winrt/base.h>
#include <winrt/microsoft.ui.xaml.h>

#include <future>

namespace OpenKneeboard {

enum class UpdateCheckType {
  Automatic,
  Manual,
};

winrt::Windows::Foundation::IAsyncAction CheckForUpdates(
  UpdateCheckType,
  const winrt::Microsoft::UI::Xaml::XamlRoot&);

}// namespace OpenKneeboard
