// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <felly/scope_exit.hpp>

namespace OpenKneeboard {

// TODO: this was originally part of Openkneeboard, but has been split out
// Perhaps update all callers to directly use `felly` in the future
using namespace felly::scope_exit_types;

}// namespace OpenKneeboard
