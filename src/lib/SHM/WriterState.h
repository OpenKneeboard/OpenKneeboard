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

#define OPENKNEEBOARD_SHM_WRITER_STATES \
  IT(Unlocked) \
  IT(TryLock) \
  IT(Locked) \
  IT(FrameInProgress) \
  IT(SubmittingFrame) \
  IT(Detaching)

enum class WriterState {
#define IT(x) x,
  OPENKNEEBOARD_SHM_WRITER_STATES
#undef IT
};

constexpr auto formattable_state(WriterState state) noexcept {
  switch (state) {
#define IT(x) \
  case WriterState::x: \
    return "WriterState::" #x;
    OPENKNEEBOARD_SHM_WRITER_STATES
#undef IT
  }
  OPENKNEEBOARD_LOG_AND_FATAL(
    "Invalid SHM WriterState: {}",
    static_cast<std::underlying_type_t<WriterState>>(state));
}
}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {

OPENKNEEBOARD_DECLARE_LOCKABLE_STATE_TRANSITIONS(SHM::WriterState)
#define IT(IN, OUT) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION( \
    SHM::WriterState::IN, SHM::WriterState::OUT)
IT(Locked, FrameInProgress)
IT(FrameInProgress, SubmittingFrame)
IT(SubmittingFrame, Locked)
IT(Locked, Detaching)
IT(Detaching, Locked)
#undef IT

static_assert(lockable_state<SHM::WriterState>);

}// namespace OpenKneeboard