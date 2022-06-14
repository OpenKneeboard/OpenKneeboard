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

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <shims/winrt.h>

#include <stop_token>
#include <string>

namespace OpenKneeboard {

void dprint(std::string_view s);
void dprint(std::wstring_view s);

template <typename... T>
void dprintf(fmt::format_string<T...> fmt, T&&... args) {
  dprint(fmt::format(fmt, std::forward<T>(args)...));
}

template <typename... T>
void dprintf(fmt::wformat_string<T...> fmt, T&&... args) {
  dprint(fmt::vformat(fmt::wstring_view(fmt), fmt::make_wformat_args(args...)));
}

struct DPrintSettings {
  enum class ConsoleOutputMode {
    DEBUG_ONLY,
    ALWAYS,
  };

  std::string prefix = "OpenKneeboard";
  ConsoleOutputMode consoleOutput = ConsoleOutputMode::DEBUG_ONLY;

  static void Set(const DPrintSettings&);
};

#pragma pack(push)
struct DPrintMessageHeader {
  DWORD mProcessID = 0;
  wchar_t mExecutable[MAX_PATH];
  wchar_t mPrefix[MAX_PATH];
};
static_assert(sizeof(DPrintMessageHeader) % sizeof(wchar_t) == 0);

struct DPrintMessage : public DPrintMessageHeader {
 private:
  static constexpr auto HeaderSize = sizeof(DPrintMessageHeader);
  static constexpr auto MaxMessageSize = 4096 - HeaderSize;

 public:
  static constexpr auto MaxMessageLength = MaxMessageSize / sizeof(wchar_t);
  wchar_t mMessage[MaxMessageLength];
};
#pragma pack(pop)

class DPrintReceiver {
 public:
  DPrintReceiver();
  ~DPrintReceiver();

  bool IsUsable() const;
  void Run(std::stop_token);

 protected:
  virtual void OnMessage(const DPrintMessage&) = 0;

 private:
  winrt::handle mMutex;
  winrt::handle mMapping;
  winrt::handle mBufferReadyEvent;
  winrt::handle mDataReadyEvent;
  DPrintMessage* mSHM = nullptr;

  bool mUsable = false;
};

}// namespace OpenKneeboard
