// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEventServer.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/task/resume_on_signal.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

namespace OpenKneeboard {

std::shared_ptr<APIEventServer> APIEventServer::Create() {
  auto ret = shared_with_final_release(new APIEventServer());
  ret->Start();
  return ret;
}

OpenKneeboard::fire_and_forget APIEventServer::final_release(
  std::unique_ptr<APIEventServer> self) {
  TraceLoggingWrite(gTraceProvider, "APIEventServer::final_release()");
  self->mStop.request_stop();
  co_await std::move(self->mRunner).value();
  self = {};
  TraceLoggingWrite(gTraceProvider, "APIEventServer::~final_release()");
}

APIEventServer::APIEventServer() {
  OPENKNEEBOARD_TraceLoggingScope("APIEventServer::APIEventServer()");
  dprint("{}", __FUNCTION__);
}

void APIEventServer::Start() {
  mStop = {};
  mRunner = this->Run();
}

APIEventServer::~APIEventServer() {
  OPENKNEEBOARD_TraceLoggingScope("APIEventServer::~APIEventServer()");
  dprint("{}", __FUNCTION__);
}

task<void> APIEventServer::Run() {
  auto weak = weak_from_this();
  auto stop = mStop.get_token();
  const auto mailslot = Win32::CreateMailslot(
    APIEvent::GetMailslotPath(), 0, MAILSLOT_WAIT_FOREVER, nullptr);
  if (!mailslot) {
    dprint("Failed to create APIEvent mailslot: {}", mailslot.error());
    co_return;
  }

  dprint("Started listening for API events");
  const scope_exit logOnExit([]() {
    dprint(
      "APIEventServer shutting down with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  const auto event =
    Win32::or_throw::CreateEvent(nullptr, FALSE, FALSE, nullptr);

  try {
    while ((!stop.stop_requested())
           && co_await RunSingle(weak, event.get(), mailslot->get())) {
      // repeat!
    }
  } catch (const winrt::hresult_canceled&) {
    co_return;
  }

  co_return;
}

task<bool> APIEventServer::RunSingle(
  std::weak_ptr<APIEventServer> instance,
  HANDLE notifyEvent,
  HANDLE mailslot) {
  auto stop = instance.lock()->mStop.get_token();
  OVERLAPPED overlapped {.hEvent = notifyEvent};

  /* If there's no message yet, use this buffer size.
   *
   * If the buffer is too small, we'll have '0 bytes read',
   * and loop into this function again.
   *
   * We'll then get a good result from GetMailslotInfo(),
   * and allocate a correctly-sized buffer.
   */
  const constexpr DWORD DefaultBufferSize = 4096;
  DWORD bufferSize {DefaultBufferSize};
  if (
    (!GetMailslotInfo(mailslot, nullptr, &bufferSize, nullptr, nullptr))
    || bufferSize == MAILSLOT_NO_MESSAGE) {
    bufferSize = DefaultBufferSize;
  }

  std::vector<char> buffer(bufferSize);
  DWORD bytesRead {};
  const auto readFileResult = ReadFile(
    mailslot,
    buffer.data(),
    static_cast<DWORD>(buffer.size()),
    &bytesRead,
    &overlapped);
  const auto readFileError = GetLastError();
  if ((!readFileResult) && readFileError != ERROR_IO_PENDING) {
    dprint("APIEvent ReadFile failed: {}", GetLastError());
    co_return true;
  }

  TraceLoggingWrite(gTraceProvider, "APIEventServer::RunSingle()/Wait");
  if (!co_await resume_on_signal(notifyEvent, stop)) {
    co_return false;
  }
  GetOverlappedResult(mailslot, &overlapped, &bytesRead, TRUE);

  if (bytesRead == 0) {
    dprint("Read 0-byte APIEvent message");
    co_return true;
  }

  auto self = instance.lock();
  if (!self) {
    dprint("Failed to acquire self");
    co_return false;
  }

  self->DispatchEvent({buffer.data(), bytesRead});
  co_return true;
}

OpenKneeboard::fire_and_forget APIEventServer::DispatchEvent(
  std::string_view ref) {
  const auto stayingAlive = shared_from_this();
  const std::string buffer(ref);
  auto event = APIEvent::Unserialize(buffer);

  co_await mUIThread;
  OPENKNEEBOARD_TraceLoggingCoro(
    "APIEvent", TraceLoggingValue(event.name.c_str(), "Name"));
  if (event.name != APIEvent::EVT_MULTI_EVENT) {
    this->evAPIEvent.Emit(event);
    co_return;
  }

  std::vector<std::tuple<std::string, std::string>> events;
  events = nlohmann::json::parse(event.value);
  auto dq = DispatcherQueue::GetForCurrentThread();
  for (auto&& [name, value]: events) {
    OPENKNEEBOARD_TraceLoggingCoro(
      "APIEvent/Multi", TraceLoggingValue(name.c_str(), "Name"));
    this->evAPIEvent.Emit({name, value});
    // Re-enter event loop, even if not switching threads
    co_await wil::resume_foreground(dq);
  }

  co_return;
}

}// namespace OpenKneeboard
