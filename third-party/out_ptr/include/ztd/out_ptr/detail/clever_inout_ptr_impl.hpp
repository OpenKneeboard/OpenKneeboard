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

#ifndef ZTD_OUT_PTR_DETAIL_CLEVER_INOUT_PTR_IMPL_HPP
#define ZTD_OUT_PTR_DETAIL_CLEVER_INOUT_PTR_IMPL_HPP

#include <ztd/out_ptr/version.hpp>

#include <ztd/out_ptr/detail/base_inout_ptr_impl.hpp>
#include <ztd/out_ptr/detail/is_specialization_of.hpp>
#include <ztd/out_ptr/detail/voidpp_op.hpp>
#include <ztd/out_ptr/pointer_of.hpp>
#include <ztd/out_ptr/detail/integer_sequence.hpp>

#include <memory>
#include <tuple>
#include <utility>
#if ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_
#include <cassert>
#endif // assert for sanity checks


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127)
#endif

namespace ztd {
namespace out_ptr {
namespace op_detail {

	template <typename Smart, typename T, typename D, typename Pointer>
	struct ZTD_OUT_PTR_TRIVIAL_ABI_I_ inout_unique_fast : voidpp_op<inout_unique_fast<Smart, T, D, Pointer>, Pointer> {
	public:
		using source_pointer = pointer_of_or_t<Smart, Pointer>;

	private:
		using can_aliasing_optimization = std::integral_constant<bool,
			sizeof(Smart) <= sizeof(Pointer) && sizeof(Smart) <= sizeof(source_pointer)>;

		Pointer* m_target_ptr;

		inout_unique_fast(std::true_type, Smart& ptr) noexcept
		: m_target_ptr(static_cast<Pointer*>(static_cast<void*>(std::addressof(ptr)))) {
			// we can assume things are cleaner here
#if ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_
			assert(*this->m_target_ptr == static_cast<Pointer>(ptr.get()) && "clever UB-based optimization did not properly retrieve the pointer value, consider turning it off for your platform");
#endif // Clever Sanity Checks
		}

		inout_unique_fast(std::false_type, Smart& ptr) noexcept {
			// analysis necessary
			void* target;
			if (is_specialization_of<Smart, boost::movelib::unique_ptr>::value) {
				// boost::movelib::unique_ptr
#if ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_
				if (std::is_reference<D>::value || std::is_function<D>::value) {
					// implementation has Pointer as first member: alias directly
					target = static_cast<void*>(std::addressof(ptr));
				}
				else {
					// implementation has Pointer as second member: shift, align, alias
					constexpr const std::size_t memory_start = sizeof(D) + (sizeof(D) % alignof(D));
					std::size_t max_space				 = sizeof(Smart) - memory_start;
					void* source						 = static_cast<void*>(static_cast<char*>(static_cast<void*>(std::addressof(ptr))) + memory_start);
					target							 = std::align(alignof(source_pointer), sizeof(source_pointer), source, max_space);
				}
#else
				if (std::is_reference<D>::value || std::is_function<D>::value) {
					// implementation has Pointer as second member: shift, align, alias
					constexpr const std::size_t memory_start = sizeof(D) + (sizeof(D) % alignof(D));
					std::size_t max_space				 = sizeof(Smart) - memory_start;
					void* source						 = static_cast<void*>(static_cast<char*>(static_cast<void*>(std::addressof(ptr))) + memory_start);
					target							 = std::align(alignof(source_pointer), sizeof(source_pointer), source, max_space);
				}
				else {
					// implementation has Pointer as first member: alias directly
					target = static_cast<void*>(std::addressof(ptr));
				}
#endif
			}
			else {
				// std::unique_ptr
#if ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_
				// implementation has Pointer as first member: alias directly
				target = static_cast<void*>(std::addressof(ptr));
#else
				// implementation has Pointer as second member: shift, align, alias
				constexpr const std::size_t memory_start = sizeof(D) + (sizeof(D) % alignof(D));
				std::size_t max_space				 = sizeof(Smart) - memory_start;
				void* source						 = static_cast<void*>(static_cast<char*>(static_cast<void*>(std::addressof(ptr))) + memory_start);
				target							 = std::align(alignof(source_pointer), sizeof(source_pointer), source, max_space);
#endif
			}
			// get direct Pointer
			this->m_target_ptr = static_cast<Pointer*>(target);
#if ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_
			assert(*this->m_target_ptr == static_cast<Pointer>(ptr.get()) && "clever UB-based optimization did not properly retrieve the pointer value, consider turning it off for your platform");
#endif // Clever Sanity Checks
		}

	public:
		inout_unique_fast(Smart& ptr, std::tuple<>&&) noexcept
		: inout_unique_fast(can_aliasing_optimization(), ptr) {
		}
		inout_unique_fast(inout_unique_fast&& right) noexcept			= default;
		inout_unique_fast& operator=(inout_unique_fast&& right) noexcept = default;

		operator Pointer*() const noexcept {
			return const_cast<Pointer*>(this->m_target_ptr);
		}
	};

	template <typename Smart, typename Pointer, typename Args, typename List, typename = void>
	class ZTD_OUT_PTR_TRIVIAL_ABI_I_ clever_inout_ptr_impl : public base_inout_ptr_impl<Smart, Pointer, Args, List> {
	private:
		using base_t = base_inout_ptr_impl<Smart, Pointer, Args, List>;

	public:
		using base_t::base_t;
	};

	// defer to unique optimization, if possible
	template <typename T, typename D, typename Pointer>
	class ZTD_OUT_PTR_TRIVIAL_ABI_I_ clever_inout_ptr_impl<std::unique_ptr<T, D>, Pointer, std::tuple<>, ztd::out_ptr::op_detail::index_sequence<>,
		typename std::enable_if<
			std::is_same<pointer_of_t<std::unique_ptr<T, D>>, Pointer>::value
			|| op_detail::has_unspecialized_marker<out_ptr_traits<std::unique_ptr<T, D>, Pointer>>::value
			|| std::is_base_of<pointer_of_t<std::unique_ptr<T, D>>, Pointer>::value
			|| !std::is_convertible<pointer_of_t<std::unique_ptr<T, D>>, Pointer>::value>::type>
	: public inout_unique_fast<std::unique_ptr<T, D>, T, D, Pointer> {
	private:
		using base_t = inout_unique_fast<std::unique_ptr<T, D>, T, D, Pointer>;

	public:
		using base_t::base_t;
	};

	template <typename T, typename D, typename Pointer>
	class ZTD_OUT_PTR_TRIVIAL_ABI_I_ clever_inout_ptr_impl<boost::movelib::unique_ptr<T, D>, Pointer, std::tuple<>, ztd::out_ptr::op_detail::index_sequence<>,
		typename std::enable_if<
			std::is_same<pointer_of_t<boost::movelib::unique_ptr<T, D>>, Pointer>::value
			|| op_detail::has_unspecialized_marker<out_ptr_traits<boost::movelib::unique_ptr<T, D>, Pointer>>::value
			|| std::is_base_of<pointer_of_t<boost::movelib::unique_ptr<T, D>>, Pointer>::value
			|| !std::is_convertible<pointer_of_t<boost::movelib::unique_ptr<T, D>>, Pointer>::value>::type>
	: public inout_unique_fast<boost::movelib::unique_ptr<T, D>, T, D, Pointer> {
	private:
		using base_t = inout_unique_fast<boost::movelib::unique_ptr<T, D>, T, D, Pointer>;

	public:
		using base_t::base_t;
	};

}}} // namespace ztd::out_ptr::op_detail

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // ZTD_OUT_PTR_DETAIL_CLEVER_INOUT_PTR_IMPL_HPP
