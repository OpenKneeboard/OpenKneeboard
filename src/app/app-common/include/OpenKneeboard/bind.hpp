/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <shims/winrt/base.h>

#ifndef OPENKNEEBOARD_BIND_ENABLE_CPPWINRT
#define OPENKNEEBOARD_BIND_ENABLE_CPPWINRT (__has_include(<winrt/base.h>))
#endif

#ifndef OPENKNEEBOARD_BIND_ENABLE_WEAK_REFS
#define OPENKNEEBOARD_BIND_ENABLE_WEAK_REFS true
#endif

#include "bind/bind_front.hpp"

#if OPENKNEEBOARD_BIND_ENABLE_CPPWINRT
#include "bind/cppwinrt/bind_winrt_context.hpp"
#include "bind/cppwinrt/discard_winrt_event_args.hpp"
#endif

#if OPENKNEEBOARD_BIND_ENABLE_WEAK_REFS
#include "bind/weak_refs.hpp"
#endif

namespace OpenKneeboard {
using namespace ::OpenKneeboard::bind;
}