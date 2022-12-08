// Copyright ⓒ 2018-2022 ThePhD.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//  See https://github.com/ThePhD/out_ptr/blob/master/docs/out_ptr.adoc for documentation.

#pragma once

#ifndef ZTD_OUT_PTR_DETAIL_INTEGER_SEQUENCE_HPP
#define ZTD_OUT_PTR_DETAIL_INTEGER_SEQUENCE_HPP

#if __cplusplus > 201400L
#include <utility>
#endif // Non-shit standards only
#include <cstddef>

namespace ztd { namespace out_ptr { namespace op_detail {

#if __cplusplus > 201400L

	template <typename T, T... Values>
	using integer_sequence = std::integer_sequence<T, Values...>;

	template <std::size_t... Values>
	using index_sequence = std::index_sequence<Values...>;

	template <typename T, T N>
	using make_integer_sequence = std::make_integer_sequence<T, N>;

	template <std::size_t N>
	using make_index_sequence = std::make_index_sequence<N>;

#else

	template <typename T, T... Values>
	class integer_sequence { };

	template <std::size_t... Values>
	using index_sequence = integer_sequence<std::size_t, Values...>;

	namespace impl {
		template <typename T, T N, bool, T... Values>
		struct mis {
			using type = typename mis<T, N - 1, ((N - 1) == 0), N - 1, Values...>::type;
		};

		template <typename T, T N, T... Values>
		struct mis<T, N, true, Values...> {
			using type = integer_sequence<T, Values...>;
		};
	} // namespace impl

	template <typename T, T N>
	using make_integer_sequence = typename impl::mis<T, N, N == 0>::type;

	template <std::size_t N>
	using make_index_sequence = make_integer_sequence<std::size_t, N>;

#endif

}}} // namespace ztd::out_ptr::op_detail

#endif // ZTD_OUT_PTR_DETAIL_INTEGER_SEQUENCE_HPP
