#pragma once

#include <fmt/format.h>

namespace OpenKneeboard {

template <typename... T>
void dprint(fmt::format_string<T...> fmt, T&&... args) {
	auto str = fmt::format(fmt, std::forward<T>(args)...);
	str = fmt::format("[openkneeboard] {}\n", str);
	OutputDebugStringA(str.c_str());
}

}
