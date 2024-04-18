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
#include <OpenKneeboard/GameEventServer.h>
#include <OpenKneeboard/Win32.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/json.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <Windows.h>

#include <thread>

namespace OpenKneeboard {

std::shared_ptr<GameEventServer> GameEventServer::Create() {
  auto ret = shared_with_final_release(new GameEventServer());
  ret->Start();
  return ret;
}

winrt::fire_and_forget GameEventServer::final_release(
  std::unique_ptr<GameEventServer> self) {
  TraceLoggingWrite(gTraceProvider, "GameEventServer::final_release()");
  self->mStop.request_stop();
  co_await winrt::resume_on_signal(self->mCompletionHandle.get());
  self = {};
  TraceLoggingWrite(gTraceProvider, "GameEventServer::~final_release()");
}

GameEventServer::GameEventServer() {
  OPENKNEEBOARD_TraceLoggingScope("GameEventServer::GameEventServer()");
  dprintf("{}", __FUNCTION__);
}

void GameEventServer::Start() {
  mStop = {};
  mRunner = this->Run();
}

GameEventServer::~GameEventServer() {
  OPENKNEEBOARD_TraceLoggingScope("GameEventServer::~GameEventServer()");
  dprintf("{}", __FUNCTION__);
}

winrt::Windows::Foundation::IAsyncAction GameEventServer::Run() {
  const scope_guard markCompletion(
    [handle = mCompletionHandle.get()]() { SetEvent(handle); });

  auto weak = weak_from_this();
  auto stop = mStop.get_token();
  const auto mailslot = Win32::CreateMailslotW(
    GameEvent::GetMailslotPath(), 0, MAILSLOT_WAIT_FOREVER, nullptr);
  if (!mailslot) {
    dprintf("Failed to create GameEvent mailslot: {}", GetLastError());
    co_return;
  }

  dprint("Started listening for game events");
  const scope_guard logOnExit([]() {
    dprintf(
      "GameEventServer shutting down with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  const auto event = Win32::CreateEventW(nullptr, FALSE, FALSE, nullptr);

  try {
    while ((!stop.stop_requested())
           && co_await RunSingle(weak, event.get(), mailslot.get())) {
      // repeat!
    }
  } catch (const winrt::hresult_canceled&) {
    co_return;
  }

  co_return;
}

winrt::Windows::Foundation::IAsyncOperation<bool> GameEventServer::RunSingle(
  std::weak_ptr<GameEventServer> instance,
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
    dprintf("GameEvent ReadFile failed: {}", GetLastError());
    co_return true;
  }

  TraceLoggingWrite(gTraceProvider, "GameEventServer::RunSingle()/Wait");
  co_await resume_on_signal(stop, notifyEvent);
  if (stop.stop_requested()) {
    co_return false;
  }
  GetOverlappedResult(mailslot, &overlapped, &bytesRead, TRUE);

  if (bytesRead == 0) {
    dprint("Read 0-byte GameEvent message");
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

winrt::fire_and_forget GameEventServer::DispatchEvent(std::string_view ref) {
  const std::string buffer(ref);

  const auto stayingAlive = shared_from_this();
  co_await winrt::resume_background();

  auto event = GameEvent::Unserialize(buffer);
  TraceLoggingActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "GameEvent", TraceLoggingValue(event.name.c_str(), "Name"));
  const scope_guard stopActivity(
    [&activity]() { TraceLoggingWriteStop(activity, "GameEvent"); });
  if (event.name != GameEvent::EVT_MULTI_EVENT) {
    this->evGameEvent.EnqueueForContext(mUIThread, event);
    co_return;
  }

  std::vector<std::tuple<std::string, std::string>> events;
  events = nlohmann::json::parse(event.value);
  for (const auto& [name, value]: events) {
    TraceLoggingActivity<gTraceProvider> subActivity;
    TraceLoggingWriteStart(
      subActivity, "GameEvent/Multi", TraceLoggingValue(name.c_str(), "Name"));
    co_await this->evGameEvent.EmitFromContextAsync(mUIThread, {name, value});
    TraceLoggingWriteStop(
      subActivity, "GameEvent/Multi", TraceLoggingValue(name.c_str(), "Name"));
  }

  co_return;
}

}// namespace OpenKneeboard
