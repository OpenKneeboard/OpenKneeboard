// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
