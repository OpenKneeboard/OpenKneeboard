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

#ifndef ZTD_OUT_PTR_DETAIL_BASE_INOUT_PTR_IMPL_HPP
#define ZTD_OUT_PTR_DETAIL_BASE_INOUT_PTR_IMPL_HPP

#include <ztd/out_ptr/detail/base_out_ptr_impl.hpp>

#include <cstdlib>
#include <type_traits>
#include <memory>
#include <utility>

namespace ztd {
namespace out_ptr {
namespace op_detail {

	template <typename Smart, typename Pointer, typename Args, typename List>
	struct base_inout_ptr_impl : base_out_ptr_impl<Smart, Pointer, inout_ptr_traits<Smart, Pointer>, Args, List> {
	private:
		using base_t = base_out_ptr_impl<Smart, Pointer, inout_ptr_traits<Smart, Pointer>, Args, List>;

	public:
		base_inout_ptr_impl(Smart& ptr, Args&& args) noexcept
		: base_t(ptr, std::move(args)) {
			static_assert(is_releasable<Smart>::value || std::is_pointer<Smart>::value, "You cannot use an inout pointer with something that cannot release() its pointer!");
		}

		base_inout_ptr_impl(base_inout_ptr_impl&& right) noexcept
		: base_t(std::move(right)) {
		}

		base_inout_ptr_impl& operator=(base_inout_ptr_impl&& right) noexcept {
			static_cast<base_t&>(*this) = std::move(right);
		}
	};

}}} // namespace ztd::out_ptr::op_detail

#endif // ZTD_OUT_PTR_DETAIL_BASE_INOUT_PTR_IMPL_HPP
