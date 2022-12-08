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

#ifndef ZTD_OUT_PTR_DETAIL_OUT_PTR_TRAITS_HPP
#define ZTD_OUT_PTR_DETAIL_OUT_PTR_TRAITS_HPP

#include <ztd/out_ptr/pointer_of.hpp>

#include <type_traits>
#include <utility>

namespace ztd {
namespace out_ptr {
	namespace op_detail {

		template <typename Smart, typename... Args>
		void reset_or_create(std::true_type, Smart& s, Args&&... args) noexcept(noexcept(s.reset(std::forward<Args>(args)...))) {
			s.reset(std::forward<Args>(args)...);
		}

		template <typename Smart, typename... Args>
		void reset_or_create(std::false_type, Smart& s, Args&&... args) noexcept(noexcept(s = Smart(std::forward<Args>(args)...))) {
			s = Smart(std::forward<Args>(args)...);
		}

		template <typename Traits, typename Smart, typename Pointer>
		auto call_traits_get(std::true_type, Smart& s, Pointer& p) noexcept -> decltype(Traits::get(std::declval<Smart&>(), std::declval<Pointer&>())) {
			return Traits::get(s, p);
		}

		template <typename Traits, typename Smart, typename Pointer>
		Pointer* call_traits_get(std::false_type, Smart&, Pointer& p) noexcept {
			return std::addressof(p);
		}

		template <typename T>
		struct has_unspecialized_marker {
			template <typename C>
			static std::true_type test(typename C::OUT_PTR_DETAIL_UNSPECIALIZED_MARKER_*);

			template <typename>
			static std::false_type test(...);

			static constexpr const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
		};

		template <typename T>
		struct has_traits_get_call {
			template <typename C>
			static std::true_type test(decltype(&C::get)*);

			template <typename>
			static std::false_type test(...);

			static constexpr const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
		};
	} // namespace op_detail

	template <typename Smart, typename Pointer>
	class out_ptr_traits {
	private:
		template <typename T>
		friend struct op_detail::has_unspecialized_marker;
		using OUT_PTR_DETAIL_UNSPECIALIZED_MARKER_ = int;
		using source_pointer				   = pointer_of_or_t<Smart, Pointer>;

	public:
		using pointer = Pointer;

		template <typename... Args>
		static pointer construct(Smart&, Args&&...) noexcept {
			return pointer {};
		}

		static typename std::add_pointer<pointer>::type get(Smart&, pointer& p) {
			return std::addressof(p);
		}

		template <typename... Args>
		static void reset(Smart& s, pointer& p, Args&&... args) noexcept {
			using can_reset = op_detail::is_resetable<Smart, source_pointer, Args...>;
			if (p) {
				reset_or_create(can_reset(), s, static_cast<source_pointer>(std::move(p)), std::forward<Args>(args)...);
			}
		}
	};

}} // namespace ztd::out_ptr

#endif // ZTD_OUT_PTR_DETAIL_OUT_PTR_TRAITS_HPP
