// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
  static OpenKneeboard::fire_and_forget final_release(
    std::unique_ptr<APIEventServer>);
  ~APIEventServer();

  Event<APIEvent> evAPIEvent;

 private:
  ProcessShutdownBlock mShutdownBlock;
  APIEventServer();
  std::optional<task<void>> mRunner;
  std::stop_source mStop;
  winrt::apartment_context mUIThread;

  void Start();

  task<void> Run();
  static task<bool>
  RunSingle(std::weak_ptr<APIEventServer>, HANDLE event, HANDLE mailslot);
  OpenKneeboard::fire_and_forget DispatchEvent(std::string_view);
};

}// namespace OpenKneeboard
