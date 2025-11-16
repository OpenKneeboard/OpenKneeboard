// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <format>
#include <functional>
#include <optional>
#include <ostream>
#include <source_location>
#include <stop_token>
#include <string>

namespace OpenKneeboard {

class DebugPrinter {
  static void Write(std::string_view);
  static void Write(std::wstring_view);

  enum class BreakWhen {
    Never,
    DebugBuilds,
    Always,
  };

  template <class TDerived, BreakWhen TBreakWhen>
  struct Level {
    void operator()(std::string_view s) const noexcept {
      Write(TDerived::Transform(s));
      this->MaybeBreak();
    }

    void operator()(std::wstring_view s) const noexcept {
      Write(TDerived::Transform(s));
      this->MaybeBreak();
    }

    template <class... Args>
      requires(sizeof...(Args) >= 1)
    void operator()(std::format_string<Args...> fmt, Args&&... args)
      const noexcept {
      this->operator()(std::format(fmt, std::forward<Args>(args)...));
    }

    template <class... Args>
      requires(sizeof...(Args) >= 1)
    void operator()(std::wformat_string<Args...> fmt, Args&&... args)
      const noexcept {
      this->operator()(std::format(fmt, std::forward<Args>(args)...));
    }

   private:
    template <class CharT>
    static auto Transform(std::basic_string_view<CharT> in) noexcept {
      return in;
    }

    void MaybeBreak() const {
      switch (TBreakWhen) {
        case BreakWhen::Never:
          break;
        case BreakWhen::Always:
          if (IsDebuggerPresent()) {
            __debugbreak();
          } else {
            OPENKNEEBOARD_BREAK;
          }
          break;
        case BreakWhen::DebugBuilds:
          OPENKNEEBOARD_BREAK;
          break;
      }
    }
  };

  struct VerboseLevel : Level<VerboseLevel, BreakWhen::Never> {};

  struct WarningLevel : Level<WarningLevel, BreakWhen::Never> {
   public:
    static auto Transform(std::string_view in) {
      return std::format("⚠️ WARNING: {}", in);
    }

    static auto Transform(std::wstring_view in) {
      return std::format(L"⚠️ WARNING: {}", in);
    }
  };

  struct ErrorLevel : Level<ErrorLevel, BreakWhen::Always> {
   public:
    static auto Transform(std::string_view in) {
      return std::format("⚠️ ERROR: {}", in);
    }

    static auto Transform(std::wstring_view in) {
      return std::format(L"⚠️ ERROR: {}", in);
    }
  };

 public:
  const VerboseLevel Verbose;
  const WarningLevel Warning;
  const ErrorLevel Error;

  void operator()(std::string_view what) const noexcept {
    Verbose(what);
  }

  void operator()(std::wstring_view what) const noexcept {
    Verbose(what);
  }

  template <class... Args>
    requires(sizeof...(Args) >= 1)
  void operator()(std::format_string<Args...> fmt, Args&&... args)
    const noexcept {
    Verbose(fmt, std::forward<Args>(args)...);
  }

  template <class... Args>
    requires(sizeof...(Args) >= 1)
  void operator()(std::wformat_string<Args...> fmt, Args&&... args)
    const noexcept {
    Verbose(fmt, std::forward<Args>(args)...);
  }

  using HistoryProvider = std::function<std::string()>;
  static void SetHistoryProvider(const HistoryProvider&);

  /** Strongly prefer TroubleshootingStore::GetDPrintDebugLogAsString().
   *
   * This interface exists so the messages can be retrieved without a hard
   * dependency on app-common, e.g. by lib/fatal
   *
   * @returns `std::nullopt` if outside the app
   */
  [[nodiscard,
    deprecated(
      "Use TroubleshootingStore::GetDPrintDebugLogAsString() instead")]]
  static std::optional<std::string> MaybeGetHistory();
};

namespace detail {
// TODO: remove once MSVC supports C++23 static operator()
template <class T>
constexpr T static_const {};
};// namespace detail

namespace {
constexpr auto const& dprint = detail::static_const<DebugPrinter>;
}

/** Utility for logging object lifetime.
 *
 * Example usage:
 *
 * ```
 * const auto lock = DPrintLifetime { std::unique_lock { (*p->mDXR) }, "Lock" };
 * ```
 *
 * Note this will log at runtime in release builds, so calls should probably not
 * be committed.
 */
template <class T>
class DPrintLifetime final {
 public:
  DPrintLifetime() = delete;
  DPrintLifetime(
    std::string_view what,
    T&& it,
    const std::source_location& loc = std::source_location::current())
    : mValue(std::move(it)),
      mWhat(what),
      mLoc(loc),
      mUncaughtExceptions(std::uncaught_exceptions()) {
    dprint(
      "{} in {}() @ {}:{}",
      mWhat,
      loc.function_name(),
      loc.file_name(),
      loc.line());
  }

  ~DPrintLifetime() {
    dprint(
      "~{} with {} new exceptions in {}() @ {}:{}",
      mWhat,
      std::uncaught_exceptions() - mUncaughtExceptions,
      mLoc.function_name(),
      mLoc.file_name(),
      mLoc.line());
  }

  operator T&() noexcept {
    return mValue;
  }

  operator const T&() const noexcept {
    return mValue;
  }

 private:
  const T mValue;
  const std::string mWhat;
  const std::source_location mLoc;
  const int mUncaughtExceptions;
};

struct DPrintSettings {
  enum class ConsoleOutputMode {
    NEVER,
    ALWAYS,
  };

  std::string prefix = "OpenKneeboard";
  ConsoleOutputMode consoleOutput = ConsoleOutputMode::NEVER;

  static void Set(const DPrintSettings&);
};

/**  If you change this structure, you *MUST* also change the version
 * in `GetDPrintResourceName()`.
 *
 * USE DEFINED-SIZE FIELDS ONLY - THIS STRUCT MUST BE
 * COMPATIBLE BETWEEN 32-BIT AND 64-BIT BINARIES.
 */
struct DPrintMessageHeader {
  DWORD mProcessID = 0;
  wchar_t mExecutable[MAX_PATH] {};
  wchar_t mPrefix[MAX_PATH] {};
};
static_assert(sizeof(DPrintMessageHeader) % sizeof(wchar_t) == 0);
static_assert(std::is_standard_layout_v<DPrintMessageHeader>);

/* If you change this structure, you *MUST* also change the version
 * in `GetDPrintResourceName()`
 *
 * USE DEFINED-SIZE FIELDS ONLY - THIS STRUCT MUST BE
 * COMPATIBLE BETWEEN 32-BIT AND 64-BIT BINARIES.
 */
struct DPrintMessage {
 public:
  using PortableSize = uint64_t;
  static constexpr PortableSize StructSize = 4096;
  static constexpr PortableSize MaxMessageLength
    = (StructSize - (sizeof(DPrintMessageHeader) + sizeof(PortableSize)))
    / sizeof(wchar_t);

  DPrintMessageHeader mHeader {};
  wchar_t mMessage[MaxMessageLength];
  PortableSize mMessageLength {};
};
static_assert(std::is_standard_layout_v<DPrintMessage>);
static_assert(sizeof(DPrintMessage) == DPrintMessage::StructSize);

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
