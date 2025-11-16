// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <Windows.h>

#include <FredEmmott/bindline.hpp>

#include <concepts>
#include <expected>
#include <functional>

// TODO: split to a separate library
namespace FredEmmott::winapi {

template <class TDerived>
struct noarg_bindable_t : FredEmmott::bindline_extension_api::bindable_t {
  template <class TFn>
  constexpr auto bind_to(TFn&& fn) const noexcept {
    return std::bind_front(
      []<class InvokeFn>(InvokeFn&& fn, auto... args) -> auto {
        return TDerived::invoke(std::forward<InvokeFn>(fn), args...);
      },
      std::forward<TFn>(fn));
  }
};

template <class TDerived>
struct result_transform_t : noarg_bindable_t<TDerived> {
  template <class TFn>
  constexpr static auto invoke(TFn&& fn, auto... args) {
    return TDerived::transform_result(std::forward<TFn>(fn)(args...));
  }

  static auto transform_result(auto) = delete;
};

template <class THandle, std::predicate<HANDLE> auto TIsValid>
struct basic_returns_handle
  : result_transform_t<basic_returns_handle<THandle, TIsValid>> {
  constexpr static std::expected<THandle, HRESULT> transform_result(
    HANDLE handle) {
    if (!TIsValid(handle)) {
      return std::unexpected(HRESULT_FROM_WIN32(GetLastError()));
    }
    return THandle {handle};
  }
};

template <class TTraits>
struct or_throw : result_transform_t<or_throw<TTraits>> {
  template <class T>
  constexpr static auto transform_result(T&& expected) {
    if (expected.has_value()) {
      return std::forward<T>(expected).value();
    }
    TTraits::throw_hresult(std::forward<T>(expected).error());
  }
};

struct or_default : result_transform_t<or_default> {
  template <class T>
  static constexpr T transform_result(std::expected<T, HRESULT>&& expected) {
    if (expected.has_value()) {
      return std::move(expected).value();
    }
    return T {};
  }
};

struct result_identity : result_transform_t<result_identity> {
  template <class T>
  static constexpr auto transform_result(std::expected<T, HRESULT>&& expected) {
    return std::move(expected);
  }
};

template <class Buffer, auto Impl, auto Success>
struct buffer_wrapper_returns_size_t {
  constexpr std::expected<Buffer, HRESULT> operator()(
    auto frontArgs,
    auto backArgs) const noexcept {
    const auto size = std::apply(
      Impl, std::tuple_cat(frontArgs, std::tuple {nullptr, 0}, backArgs));
    if (!Success(size)) {
      return std::unexpected(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (size == 0) {
      return {};
    }
    Buffer buffer;
    buffer.resize(size);
    const auto finalSize = std::apply(
      Impl,
      std::tuple_cat(frontArgs, std::tuple {buffer.data(), size}, backArgs));

    if (!Success(size)) {
      return std::unexpected(HRESULT_FROM_WIN32(GetLastError()));
    }
    buffer.resize(finalSize);
    return buffer;
  }
};

// Only taking a compile-time code page as we only care about CP_ACP and CP_UTF8
// for this library
template <int TCodePage>
std::expected<std::string, HRESULT> wide_to_narrow(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }

  return buffer_wrapper_returns_size_t<
    std::string,
    &WideCharToMultiByte,
    [](auto x) { return x > 0; }> {}(
    std::tuple {
      TCodePage,
      WC_ERR_INVALID_CHARS,
      const_cast<wchar_t*>(wide.data()),
      static_cast<int>(wide.size()),
    },
    std::tuple {nullptr, nullptr});
}

template <int TCodePage>
std::expected<std::wstring, HRESULT> narrow_to_wide(std::string_view narrow) {
  if (narrow.empty()) {
    return {};
  }

  return buffer_wrapper_returns_size_t<
    std::wstring,
    &MultiByteToWideChar,
    [](auto x) { return x > 0; }> {}(
    std::tuple {
      TCodePage,
      MB_ERR_INVALID_CHARS,
      const_cast<char*>(narrow.data()),
      static_cast<int>(narrow.size()),
    },
    std::tuple {});
}

struct utf8_traits {
  static constexpr auto to_wide = &narrow_to_wide<CP_UTF8>;
  static constexpr auto from_wide = &wide_to_narrow<CP_UTF8>;

  using string_type = std::string;
};

struct active_code_page_traits {
  static constexpr auto to_wide = &narrow_to_wide<CP_ACP>;
  static constexpr auto from_wide = &wide_to_narrow<CP_ACP>;

  using string_type = std::string;
};

struct wide_traits {
  static constexpr std::expected<std::wstring, HRESULT> to_wide(auto in) {
    return std::wstring {in};
  }
  static constexpr std::expected<std::wstring, HRESULT> from_wide(auto in) {
    return std::wstring {in};
  }

  using string_type = std::wstring;
};

struct raw_winapi_traits {
  using handle_or_null_type = HANDLE;
  using handle_or_invalid_type = HANDLE;

  // Error message marker
  struct winapi_does_not_use_exceptions_call_optional_value_instead {};
  static winapi_does_not_use_exceptions_call_optional_value_instead
  throw_hresult(const HRESULT)
    = delete;
};

struct winrt_winapi_traits {
  using handle_or_null_type = winrt::handle;
  using handle_or_invalid_type = winrt::file_handle;

  [[noreturn]] static void throw_hresult(const HRESULT hr) {
    OPENKNEEBOARD_ASSERT(FAILED(hr));
    winrt::check_hresult(hr);
    std::unreachable();
  };
};

template <
  class TTraits,
  class TErrorMapper = result_identity,
  class TStringTraits = wide_traits>
struct basic_winapi {
 private:
  template <class T>
  static constexpr std::expected<std::wstring, HRESULT> to_wide(T str) {
    if constexpr (std::same_as<T, std::nullptr_t>) {
      return {};
    } else {
      return TStringTraits::to_wide(str);
    }
  }

  template <class T>
  static constexpr std::expected<typename TStringTraits::string_type, HRESULT>
  from_wide(T str) {
    if constexpr (std::same_as<T, std::nullptr_t>) {
      return {};
    } else {
      return TStringTraits::from_wide(str);
    }
  }

  template <class T>
  static constexpr auto make_error(HRESULT hr) {
    return TErrorMapper::transform_result(
      std::expected<T, HRESULT>(std::unexpect, hr));
  }

  static constexpr auto nullable_cstr(const std::wstring& it) {
    return it.empty() ? nullptr : it.c_str();
  }

  static constexpr auto nullable_cstr(
    const std::expected<std::wstring, HRESULT>& it) {
    return nullable_cstr(it.value());
  }

 public:
  using handle_or_null_type = typename TTraits::handle_or_null_type;
  using handle_or_invalid_type = typename TTraits::handle_or_invalid_type;

  using returns_handle_or_null
    = basic_returns_handle<handle_or_null_type, std::identity {}>;
  using returns_handle_or_invalid
    = basic_returns_handle<handle_or_invalid_type, [](HANDLE handle) {
        return handle != INVALID_HANDLE_VALUE;
      }>;

  using or_throw = basic_winapi<TTraits, or_throw<TTraits>, TStringTraits>;
  using or_default = basic_winapi<TTraits, or_default, TStringTraits>;
  using expected = basic_winapi<TTraits, result_identity, TStringTraits>;

  using UTF8 = basic_winapi<TTraits, TErrorMapper, utf8_traits>;
  using ACP = basic_winapi<TTraits, TErrorMapper, active_code_page_traits>;
  using Wide = basic_winapi<TTraits, TErrorMapper, wide_traits>;

  // We could avoid spelling out the arguments here with `static inline const
  // CreateEvent = ... pipeline
  //
  // Spelling them out here to keep the same parameter coercion rules, and for
  // better IDE support

  // HANDLE or NULL
  static constexpr auto CreateEvent(
    LPSECURITY_ATTRIBUTES lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    auto lpName = nullptr) {
    const auto name = to_wide(lpName);
    if (!name.has_value()) {
      return make_error<handle_or_null_type>(name.error());
    }

    return (&::CreateEventW | returns_handle_or_null() | TErrorMapper())(
      lpEventAttributes, bManualReset, bInitialState, nullable_cstr(name));
  }

  static constexpr auto CreateFileMapping(
    HANDLE hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    auto lpName = nullptr) {
    const auto name = to_wide(lpName);
    if (!name.has_value()) {
      return make_error<handle_or_null_type>(name.error());
    }

    return (&::CreateFileMappingW | returns_handle_or_null() | TErrorMapper())(
      hFile,
      lpFileMappingAttributes,
      flProtect,
      dwMaximumSizeHigh,
      dwMaximumSizeLow,
      nullable_cstr(name));
  }

  static constexpr auto CreateMutex(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    BOOL bInitialOwner = true,
    auto lpName = nullptr) {
    const auto name = to_wide(lpName);
    if (!name.has_value()) {
      return make_error<handle_or_null_type>(name.error());
    }

    return (&::CreateMutexW | returns_handle_or_null() | TErrorMapper())(
      lpMutexAttributes, bInitialOwner, nullable_cstr(name));
  }

  static constexpr auto CreateWaitableTimer(
    LPSECURITY_ATTRIBUTES lpTimerAttributes,
    BOOL bManualReset,
    auto lpTimerName = nullptr) {
    const auto name = to_wide(lpTimerName);
    if (!name.has_value()) {
      return make_error<handle_or_null_type>(name.error());
    }

    return (
      &::CreateWaitableTimerW | returns_handle_or_null() | TErrorMapper())(
      lpTimerAttributes, bManualReset, nullable_cstr(name));
  }

  // HANDLE or INVALID_HANDLE_VALUE
  static constexpr auto CreateFile(
    auto lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
    HANDLE hTemplateFile = NULL) {
    const auto name = to_wide(lpFileName);
    if (!name.has_value()) {
      return make_error<handle_or_invalid_type>(name.error());
    }

    return (&::CreateFileW | returns_handle_or_invalid() | TErrorMapper())(
      nullable_cstr(name),
      dwDesiredAccess,
      dwShareMode,
      lpSecurityAttributes,
      dwCreationDisposition,
      dwFlagsAndAttributes,
      hTemplateFile);
  }
  static constexpr auto CreateMailslot(
    auto lpName,
    DWORD nMaxMessageSize,
    DWORD lReadTimeout,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes = nullptr) {
    const auto name = to_wide(lpName);
    if (!name.has_value()) {
      return make_error<handle_or_invalid_type>(name.error());
    }

    return (&::CreateMailslotW | returns_handle_or_invalid() | TErrorMapper())(
      nullable_cstr(name), nMaxMessageSize, lReadTimeout, lpSecurityAttributes);
  }
};

}// namespace FredEmmott::winapi

namespace OpenKneeboard {
using Win32 = ::FredEmmott::winapi::basic_winapi<
  ::FredEmmott::winapi::winrt_winapi_traits>;
};