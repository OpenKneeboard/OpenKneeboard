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

#ifndef ZTD_OUT_PTR_OUT_PTR_HPP
#define ZTD_OUT_PTR_OUT_PTR_HPP

#include <ztd/out_ptr/version.hpp>

#include <ztd/out_ptr/detail/simple_out_ptr.hpp>
#include <ztd/out_ptr/detail/clever_out_ptr.hpp>
#include <ztd/out_ptr/detail/out_ptr_traits.hpp>
#include <ztd/out_ptr/detail/marker.hpp>
#include <ztd/out_ptr/pointer_of.hpp>

#include <type_traits>
#include <tuple>

namespace ztd { namespace out_ptr {

	namespace op_detail {

#if ZTD_OUT_PTR_USE_CLEVER_OUT_PTR_I_
		// One must opt into this specifically because it is unsafe for a large
		// variety of APIs
		template <typename Smart, typename Pointer, typename... Args>
		using core_out_ptr_t = clever_out_ptr_t<Smart, Pointer, Args...>;
#else
		// we can never use the clever version by default
		// because many C APIs do not set
		// the pointer to null on parameter failure
		template <typename Smart, typename Pointer, typename... Args>
		using core_out_ptr_t = simple_out_ptr_t<Smart, Pointer, Args...>;
#endif // ZTD_OUT_PTR_USE_CLEVER_OUT_PTR
	} // namespace op_detail

	template <typename Smart, typename Pointer, typename... Args>
	class ZTD_OUT_PTR_TRIVIAL_ABI_I_ out_ptr_t : public op_detail::core_out_ptr_t<Smart, Pointer, Args...> {
	private:
		using base_t = op_detail::core_out_ptr_t<Smart, Pointer, Args...>;

	public:
		using base_t::base_t;
	};

	namespace op_detail {
		template <typename Pointer, typename Smart, typename... Args>
		out_ptr_t<Smart, Pointer, Args...> out_ptr_tagged(std::false_type, Smart& s, Args&&... args) noexcept(::std::is_nothrow_constructible<out_ptr_t<Smart, Pointer, Args...>, Smart&, Args...>::value) {
			using P = out_ptr_t<Smart, Pointer, Args...>;
			return P(s, std::forward<Args>(args)...);
		}

		template <typename, typename Smart, typename... Args>
		out_ptr_t<Smart, pointer_of_t<Smart>, Args...> out_ptr_tagged(std::true_type, Smart& s, Args&&... args) noexcept(::std::is_nothrow_constructible<out_ptr_t<Smart, pointer_of_t<Smart>, Args...>, Smart&, Args...>::value) {
			using Pointer = pointer_of_t<Smart>;
			using P	    = out_ptr_t<Smart, Pointer, Args...>;
			return P(s, std::forward<Args>(args)...);
		}

	} // namespace op_detail

	template <typename Pointer = op_detail::marker, typename Smart, typename... Args>
	auto out_ptr(Smart& s, Args&&... args) noexcept(noexcept(op_detail::out_ptr_tagged<Pointer>(::std::is_same<Pointer, op_detail::marker>(), s, std::forward<Args>(args)...)))
		-> decltype(op_detail::out_ptr_tagged<Pointer>(::std::is_same<Pointer, op_detail::marker>(), s, std::forward<Args>(args)...)) {
		return op_detail::out_ptr_tagged<Pointer>(::std::is_same<Pointer, op_detail::marker>(), s, std::forward<Args>(args)...);
	}

}} // namespace ztd::out_ptr

#endif // ZTD_OUT_PTR_OUT_PTR_HPP
