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

#include <OpenKneeboard/StateMachine.h>

namespace OpenKneeboard::SHM {

#define OPENKNEEBOARD_SHM_READER_STATES \
  IT(Unlocked) \
  IT(TryLock) \
  IT(Locked) \
  IT(CreatingSnapshot)

enum class ReaderState {
#define IT(x) x,
  OPENKNEEBOARD_SHM_READER_STATES
#undef IT
};

constexpr auto formattable_state(ReaderState state) noexcept {
  switch (state) {
#define IT(x) \
  case ReaderState::x: \
    return "ReaderState::" #x;
    OPENKNEEBOARD_SHM_READER_STATES
#undef IT
  }
  OPENKNEEBOARD_LOG_AND_FATAL(
    "Invalid SHM ReaderState: {}",
    static_cast<std::underlying_type_t<ReaderState>>(state));
}
}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {

OPENKNEEBOARD_DECLARE_LOCKABLE_STATE_TRANSITIONS(SHM::ReaderState)

#define IT(IN, OUT) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION( \
    SHM::ReaderState::IN, SHM::ReaderState::OUT)
IT(Locked, CreatingSnapshot)
IT(CreatingSnapshot, Locked)
#undef IT

static_assert(lockable_state<SHM::ReaderState>);

}// namespace OpenKneeboard