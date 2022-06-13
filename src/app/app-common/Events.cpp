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
#include <OpenKneeboard/Events.h>

namespace OpenKneeboard {

static EventContext sNextEventContext = 1 << 16;

EventContext CreateEventContext() {
  return sNextEventContext++;
}

void EventBase::Add(EventReceiver* receiver, uint64_t token) {
  receiver->mSenders.push_back({this, token});
}

EventReceiver::EventReceiver() {
}

EventReceiver::~EventReceiver() {
  while (!mSenders.empty()) {
    auto sender = mSenders.back();
    mSenders.pop_back();
    sender.mEvent->RemoveHandler(sender.mToken);
  }
}

}// namespace OpenKneeboard
