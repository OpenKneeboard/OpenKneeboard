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

#ifndef ZTD_OUT_PTR_DETAIL_VOIDPP_OP_HPP
#define ZTD_OUT_PTR_DETAIL_VOIDPP_OP_HPP

namespace ztd { namespace out_ptr { namespace op_detail {

	template <typename T, typename P>
	class voidpp_op {
	public:
		operator void**() const noexcept(noexcept(static_cast<P*>(std::declval<T&>()))) {
			const T& self = *static_cast<T const*>(this);
			return static_cast<void**>(static_cast<void*>(static_cast<P*>(self)));
		};
	};

	template <typename T>
	class voidpp_op<T, void*> { };

	template <typename T>
	class voidpp_op<T, const void*> { };

	template <typename T>
	class voidpp_op<T, const volatile void*> { };

}}} // namespace ztd::out_ptr::op_detail

#endif // ZTD_OUT_PTR_DETAIL_VOIDPP_OP_HPP
