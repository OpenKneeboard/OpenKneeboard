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
enum class WriterState {
  Unlocked,
  TryLock,
  Locked,
  FrameInProgress,
  SubmittingFrame,
  Detaching,
  Detached,
};
}

namespace OpenKneeboard {

OPENKNEEBOARD_DECLARE_LOCKABLE_STATE_TRANSITIONS(SHM::WriterState)
#define IT(IN, OUT) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION( \
    SHM::WriterState::IN, SHM::WriterState::OUT)
IT(Locked, FrameInProgress)
IT(FrameInProgress, SubmittingFrame)
IT(SubmittingFrame, Locked)
IT(Unlocked, Detaching)
IT(Detaching, Detached)
#undef IT

static_assert(lockable_state<SHM::WriterState>);

}// namespace OpenKneeboard