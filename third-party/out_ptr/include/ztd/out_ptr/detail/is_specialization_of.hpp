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

#ifndef ZTD_OUT_PTR_DETAIL_IS_SPECIALIZATION_OF_HPP
#define ZTD_OUT_PTR_DETAIL_IS_SPECIALIZATION_OF_HPP

#include <type_traits>

namespace ztd {
namespace out_ptr {
namespace op_detail {

	namespace meta_detail {
		template <typename T, template <typename...> class Templ>
		struct is_specialization_of : std::false_type {
		};
		template <typename... T, template <typename...> class Templ>
		struct is_specialization_of<Templ<T...>, Templ> : std::true_type {
		};

	} // namespace meta_detail

	template <typename T, template <typename...> class Templ>
	using is_specialization_of = meta_detail::is_specialization_of<typename std::remove_cv<T>::type, Templ>;

}}} // namespace ztd::out_ptr::op_detail

#endif // ZTD_OUT_PTR_DETAIL_IS_SPECIALIZATION_OF_HPP
