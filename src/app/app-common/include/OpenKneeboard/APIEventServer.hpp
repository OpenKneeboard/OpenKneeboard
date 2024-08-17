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

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <memory>

namespace OpenKneeboard {

class APIEventServer final
  : public std::enable_shared_from_this<APIEventServer> {
 public:
  static std::shared_ptr<APIEventServer> Create();
  static winrt::fire_and_forget final_release(std::unique_ptr<APIEventServer>);
  ~APIEventServer();

  Event<APIEvent> evAPIEvent;

 private:
  ProcessShutdownBlock mShutdownBlock;
  APIEventServer();
  std::optional<task<void>> mRunner;
  std::stop_source mStop;
  winrt::apartment_context mUIThread;
  winrt::handle mCompletionHandle {
    Win32::CreateEventW(nullptr, TRUE, FALSE, nullptr)};

  void Start();

  task<void> Run();
  static winrt::Windows::Foundation::IAsyncOperation<bool>
  RunSingle(std::weak_ptr<APIEventServer>, HANDLE event, HANDLE mailslot);
  winrt::fire_and_forget DispatchEvent(std::string_view);
};

}// namespace OpenKneeboard
