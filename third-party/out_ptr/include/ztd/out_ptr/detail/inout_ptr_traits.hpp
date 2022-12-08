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

#ifndef ZTD_OUT_PTR_DETAIL_INOUT_PTR_TRAITS_HPP
#define ZTD_OUT_PTR_DETAIL_INOUT_PTR_TRAITS_HPP

#include <ztd/out_ptr/detail/out_ptr_traits.hpp>
#include <ztd/out_ptr/pointer_of.hpp>

#include <type_traits>

namespace ztd {
namespace out_ptr {

	namespace op_detail {
		template <typename Smart>
		void call_release(std::true_type, Smart& s) noexcept(noexcept(std::declval<Smart&>().release())) {
			s.release();
		}

		template <typename Smart>
		void call_release(std::false_type, Smart&) noexcept {
			static_assert(std::is_pointer<Smart>::value, "the type that does not have release called on it must be a pointer type");
		}
	} // namespace op_detail

	template <typename Smart, typename Pointer>
	class inout_ptr_traits {
	public:
		using pointer = Pointer;

	private:
		using OUT_PTR_DETAIL_UNSPECIALIZED_MARKER_ = int;
		using defer_t						   = out_ptr_traits<Smart, Pointer>;

		template <typename... Args>
		static pointer construct(std::true_type, Smart& s, Args&&...) noexcept {
			return static_cast<pointer>(s);
		}

		template <typename... Args>
		static pointer construct(std::false_type, Smart& s, Args&&...) noexcept {
			return static_cast<pointer>(s.get());
		}

	public:
		template <typename... Args>
		static pointer construct(Smart& s, Args&&... args) noexcept {
			return construct(std::is_pointer<Smart>(), s, std::forward<Args>(args)...);
		}

		static typename std::add_pointer<pointer>::type get(Smart&, pointer& p) noexcept {
			return std::addressof(p);
		}

		template <typename... Args>
		static void reset(Smart& s, pointer& p, Args&&... args) noexcept {
			op_detail::call_release(op_detail::is_releasable<Smart>(), s);
			defer_t::reset(s, p, std::forward<Args>(args)...);
		}
	};

}} // namespace ztd::out_ptr

#endif // ZTD_OUT_PTR_DETAIL_INOUT_PTR_TRAITS_HPP
