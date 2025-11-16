// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <concepts>
#include <memory>
#include <type_traits>

namespace OpenKneeboard {

/** Extension of `std::enable_shared_from_this()` with subclassing support.
 *
 * This allows descendants of non-template derived classes to get a correctly
 * typed pointer.
 */
template <class Base>
class enable_shared_from_this : public std::enable_shared_from_this<Base> {
 public:
  template <class Self>
  auto shared_from_this(this Self& self) try {
    using decayed_base_t = std::enable_shared_from_this<Base>;
    using base_t = std::conditional_t<
      std::is_const_v<Self>,
      const decayed_base_t,
      decayed_base_t>;

    auto parent = static_cast<base_t&>(self).shared_from_this();
    return static_pointer_cast<std::remove_reference_t<Self>>(parent);
  } catch (const std::bad_weak_ptr&) {
    // Calling shared_from_this() from an object that does not yet have a
    // shared_ptr()
    //
    // You probably want two-phase construction.
    OPENKNEEBOARD_BREAK;
    throw;
  }

  template <class Self>
  auto weak_from_this(this Self& self) {
    return std::weak_ptr {self.shared_from_this()};
  }
};
}// namespace OpenKneeboard