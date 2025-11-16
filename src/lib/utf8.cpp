// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <icu.h>

namespace OpenKneeboard {

std::string to_utf8(const std::filesystem::path& in) {
  const auto yaycpp20 = in.u8string();
  return {reinterpret_cast<const char*>(yaycpp20.data()), yaycpp20.size()};
}

std::string to_utf8(const std::wstring& in) {
  return to_utf8(std::wstring_view {in});
}

std::string to_utf8(std::wstring_view in) {
  if (in.size() == 0) {
    return {};
  }

  UErrorCode ec {};
  int32_t length = 0;
  u_strToUTF8(
    nullptr,
    0,
    &length,
    reinterpret_cast<const UChar*>(in.data()),
    in.size(),
    &ec);
  OPENKNEEBOARD_ASSERT(
    U_SUCCESS(ec) || ec == U_BUFFER_OVERFLOW_ERROR,
    "u_strToUTF8 failed with {}",
    std::to_underlying(ec));

  if (length == 0) {
    return {};
  }
  ec = {};

  std::string ret;
  ret.resize(length);
  u_strToUTF8(
    ret.data(),
    ret.size(),
    &length,
    reinterpret_cast<const UChar*>(in.data()),
    in.size(),
    &ec);
  OPENKNEEBOARD_ASSERT(
    U_SUCCESS(ec), "u_strToUTF8 failed with {}", std::to_underlying(ec));
  OPENKNEEBOARD_ASSERT(length == ret.size());
  return ret;
}

std::string to_utf8(const wchar_t* in) {
  return to_utf8(std::wstring_view(in));
}

class UTF8CaseMap {
 public:
  UTF8CaseMap(const char locale[]) {
    auto error = U_ZERO_ERROR;
    mImpl = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &error);
  }

  ~UTF8CaseMap() {
    if (!mImpl) {
      return;
    }

    ucasemap_close(mImpl);
  }

  inline operator const UCaseMap*() const noexcept {
    return mImpl;
  }

  UTF8CaseMap() = delete;
  UTF8CaseMap(const UTF8CaseMap&) = delete;
  UTF8CaseMap(UTF8CaseMap&&) = delete;
  UTF8CaseMap& operator=(const UTF8CaseMap&) = delete;
  UTF8CaseMap& operator=(UTF8CaseMap&&) = delete;

 private:
  UCaseMap* mImpl {nullptr};
};

static UTF8CaseMap gFoldCaseMap("");

std::string fold_utf8(std::string_view in) {
  auto error = U_ZERO_ERROR;
  const auto foldedLength = ucasemap_utf8FoldCase(
    gFoldCaseMap,
    nullptr,
    0,
    in.data(),
    static_cast<int32_t>(in.size()),
    &error);

  std::string ret(static_cast<size_t>(foldedLength), '\0');
  error = U_ZERO_ERROR;
  ucasemap_utf8FoldCase(
    gFoldCaseMap,
    ret.data(),
    static_cast<int32_t>(ret.size()),
    in.data(),
    static_cast<int32_t>(in.size()),
    &error);
  return ret;
}

}// namespace OpenKneeboard

NLOHMANN_JSON_NAMESPACE_BEGIN

void adl_serializer<std::filesystem::path>::from_json(
  const nlohmann::json& j,
  std::filesystem::path& p) {
  const auto utf8 = j.get<std::string>();
  const auto first = reinterpret_cast<const char8_t*>(utf8.data());

  p = std::filesystem::path {first, first + utf8.size()};
  // forward slashes to backslashes
  p.make_preferred();
}
void adl_serializer<std::filesystem::path>::to_json(
  nlohmann::json& j,
  const std::filesystem::path& p) {
  j = OpenKneeboard::to_utf8(p);
}

NLOHMANN_JSON_NAMESPACE_END
