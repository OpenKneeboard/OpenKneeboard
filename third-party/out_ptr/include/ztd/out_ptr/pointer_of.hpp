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

#ifndef ZTD_OUT_PTR_POINTER_OF_HPP
#define ZTD_OUT_PTR_POINTER_OF_HPP

#include <type_traits>
#include <memory>

namespace ztd {
namespace out_ptr {

	namespace op_detail {

		template <typename... Ts>
		struct make_void { typedef void type; };
		template <typename... Ts>
		using void_t = typename make_void<Ts...>::type;

		template <typename T, typename U, typename = void>
		struct element_type {
			using type = U;
		};

		template <typename T, typename U>
		struct element_type<T, U, op_detail::void_t<typename T::element_type>> {
			using type = typename T::element_type*;
		};

		template <typename T, typename U, typename = void>
		struct pointer_of_or : element_type<T, U> {
		};

		template <typename T, typename U>
		struct pointer_of_or<T, U, op_detail::void_t<typename T::pointer>> {
			using type = typename T::pointer;
		};

		template <typename T>
		struct has_typedef_pointer {
			template <typename C>
			static std::true_type& test(typename C::pointer*);

			template <typename>
			static std::false_type& test(...);

			static constexpr const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
		};

		template <bool b, typename T, typename Fallback>
		struct pointer_typedef_enable_if {
		};

		template <typename T, typename Fallback>
		struct pointer_typedef_enable_if<false, T, Fallback> {
			typedef Fallback type;
		};

		template <typename T, typename Fallback>
		struct pointer_typedef_enable_if<true, T, Fallback> {
			typedef typename T::pointer type;
		};

		template <typename T, typename... Args>
		struct is_resetable_test {
		private:
			template <typename S>
			static std::true_type f(decltype(std::declval<S>().reset(std::declval<Args>()...))*);
			template <typename S>
			static std::false_type f(...);

		public:
			constexpr static bool value = std::is_same<decltype(f<T>(0)), std::true_type>::value;
		};

		template <typename T, typename = void>
		struct is_releasable : std::false_type {
		};

		template <typename T>
		struct is_releasable<T, op_detail::void_t<decltype(std::declval<T&>().release())>> : std::true_type {
		};

		template <typename T, typename... Args>
		struct is_resetable : std::integral_constant<bool, is_resetable_test<T, Args...>::value> {
		};

	} // namespace op_detail

	template <typename T, typename U>
	struct pointer_of_or : op_detail::pointer_of_or<T, U> {
	};

	template <typename T, typename U>
	using pointer_of_or_t = typename pointer_of_or<T, U>::type;

	template <typename T>
	using pointer_of = pointer_of_or<T, typename std::pointer_traits<T>::element_type*>;

	template <typename T>
	using pointer_of_t = typename pointer_of<T>::type;

	template <typename T, typename Dx>
	struct pointer_type {
		typedef typename op_detail::pointer_typedef_enable_if<op_detail::has_typedef_pointer<Dx>::value, Dx, T>::type type;
	};

	template <typename T, typename D>
	using pointer_type_t = typename pointer_type<T, D>::type;

}} // namespace ztd::out_ptr

#endif // ZTD_OUT_PTR_POINTER_OF_HPP
