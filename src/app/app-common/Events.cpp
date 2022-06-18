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

static uint64_t sNextUniqueID = 0x1234abcdui64 << 32;

uint64_t _UniqueIDImpl::GetAndIncrementNextValue() {
  return sNextUniqueID++;
}

EventHandlerToken EventBase::AddHandler(EventReceiver* receiver) {
  const EventHandlerToken token;
  receiver->mSenders.push_back({this, token});
  return token;
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

void EventReceiver::RemoveEventListener(EventHandlerToken token) {
  auto it = std::ranges::find(
    mSenders, token, [](const SenderInfo& sender) { return sender.mToken; });
  if (it == mSenders.end()) {
    return;
  }
  it->mEvent->RemoveHandler(token);
  mSenders.erase(it);
}

}// namespace OpenKneeboard
