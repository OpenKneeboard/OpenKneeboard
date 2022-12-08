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

#ifndef ZTD_OUT_PTR_VERSION_HPP
#define ZTD_OUT_PTR_VERSION_HPP

#if defined(ZTD_OUT_PTR_CLEVER_SANITY_CHECK)
#if (ZTD_OUT_PTR_CLEVER_SANITY_CHECK != 0)
#define ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_ 1
#else
#define ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_ 0
#endif
#else
#define ZTD_OUT_PTR_CLEVER_SANITY_CHECK_I_ 0
#endif

#if defined(ZTD_OUT_PTR_USE_CLEVER_OUT_PTR)
#if (ZTD_OUT_PTR_USE_CLEVER_OUT_PTR != 0)
#define ZTD_OUT_PTR_USE_CLEVER_OUT_PTR_I_ 1
#else
#define ZTD_OUT_PTR_USE_CLEVER_OUT_PTR_I_ 0
#endif
#else
#define ZTD_OUT_PTR_USE_CLEVER_OUT_PTR_I_ 0
#endif

#if defined(ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR)
#if (ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR != 0)
#define ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR_I_ 1
#else
#define ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR_I_ 0
#endif
#elif defined(_LIBCPP_VERSION) || defined(__GLIBC__) || defined(_YVALS) || defined(_CPPLIB_VER)
#define ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR_I_ 1
#else
#define ZTD_OUT_PTR_USE_CLEVER_INOUT_PTR_I_ 0
#endif


#if defined(ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER)
#if (ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER != 0)
#define ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 1
#else
#define ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 0
#endif
#elif defined(_LIBCPP_VERSION)
#define ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 1
#else
#define ZTD_OUT_PTR_CLEVER_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 0
#endif

#if defined(ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER)
#if (ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER != 0)
#define ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 1
#else
#define ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 0
#endif
#else
#define ZTD_OUT_PTR_CLEVER_MOVELIB_UNIQUE_IMPLEMENTATION_FIRST_MEMBER_I_ 0
#endif

// Only defined for clang version 7 and above
#if defined(ZTD_OUT_PTR_TRIVIAL_ABI)
#define ZTD_OUT_PTR_TRIVIAL_ABI_I_ ZTD_OUT_PTR_TRIVIAL_ABI
#elif defined(__clang__) && __clang__ >= 7
#define ZTD_OUT_PTR_TRIVIAL_ABI_I_ __attribute__((trivial_abi))
#else
#define ZTD_OUT_PTR_TRIVIAL_ABI_I_
#endif // Clang or otherwise

#endif // ZTD_OUT_PTR_VERSION_HPP
