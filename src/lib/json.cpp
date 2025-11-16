// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/json.hpp>

using namespace OpenKneeboard::detail::SparseJson;

namespace OpenKneeboard {

static_assert(
  ConstStrLowerFirst(Wrap("FooBar")).mBuffer == std::string_view {"fooBar"});
static_assert(
  ConstStrSkipFirst(Wrap("mFooBar")).mBuffer == std::string_view {"FooBar"});
static_assert(
  ConstStrSkipFirstLowerNext(Wrap("mFooBar")).mBuffer
  == std::string_view {"fooBar"});

}// namespace OpenKneeboard

namespace nlohmann {
void adl_serializer<winrt::guid>::to_json(json& j, const winrt::guid& v) {
  j = winrt::to_string(winrt::to_hstring(v));
}
void adl_serializer<winrt::guid>::from_json(const json& j, winrt::guid& v) {
  v = winrt::guid {j.get<std::string>()};
}
}// namespace nlohmann
