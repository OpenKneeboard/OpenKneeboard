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

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/array.hpp>

namespace OpenKneeboard::SHM {

#define OPENKNEEBOARD_SHM_WRITER_STATES \
  IT(Unlocked) \
  IT(TryLock) \
  IT(Locked) \
  IT(FrameInProgress) \
  IT(SubmittingFrame) \
  IT(SubmittingEmptyFrame) \
  IT(Detaching)

enum class WriterState {
#define IT(x) x,
  OPENKNEEBOARD_SHM_WRITER_STATES
#undef IT
};

using WriterStateMachine = StateMachine<
  WriterState,
  WriterState::Unlocked,
  array_cat(
    lockable_transitions<WriterState>(),
    std::array {
      Transition {WriterState::Locked, WriterState::FrameInProgress},
      Transition {WriterState::FrameInProgress, WriterState::SubmittingFrame},
      Transition {WriterState::SubmittingFrame, WriterState::Locked},
      Transition {WriterState::Locked, WriterState::SubmittingEmptyFrame},
      Transition {WriterState::SubmittingEmptyFrame, WriterState::Locked},
      Transition {WriterState::Locked, WriterState::Detaching},
      Transition {WriterState::Detaching, WriterState::Locked},
    }),
  WriterState::Unlocked>;

static_assert(lockable_state_machine<SHM::WriterStateMachine>);

}// namespace OpenKneeboard::SHM