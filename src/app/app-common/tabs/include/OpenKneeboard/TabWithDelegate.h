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

#include <OpenKneeboard/Events.h>

#include <concepts>
#include <type_traits>

#include "Tab.h"
#include "TabWithDoodles.h"
#include "TabWithNavigation.h"

namespace OpenKneeboard {

template <std::derived_from<Tab> T>
class TabWithDelegateBase : public virtual Tab {
 public:
  virtual utf8_string GetTitle() const override {
    return this->GetDelegate()->GetTitle();
  }

  virtual uint16_t GetPageCount() const override {
    return this->GetDelegate()->GetPageCount();
  }

  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override {
    return this->GetDelegate()->GetNativeContentSize(pageIndex);
  }

  virtual void RenderPage(
    ID2D1DeviceContext* device,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override {
    this->GetDelegate()->RenderPage(device, pageIndex, rect);
  }

  virtual void Reload() override {
    this->GetDelegate()->Reload();
  }

 protected:
  virtual T* GetDelegate() const = 0;
};

template <class T>
class TabWithCursorEventsDelegate : public virtual TabWithCursorEvents,
                                    public virtual TabWithDelegateBase<T> {
 public:
  virtual void PostCursorEvent(
    EventContext ctx,
    const CursorEvent& ev,
    uint16_t pageIndex) override {
    this->GetDelegate()->PostCursorEvent(ctx, ev, pageIndex);
  }
};

template <class T>
class TabWithNavigationDelegate : public virtual TabWithNavigation,
                                  public virtual TabWithDelegateBase<T> {
 public:
  virtual bool IsNavigationAvailable() const {
    return this->GetDelegate()->IsNavigationAvailable();
  }

  virtual std::shared_ptr<Tab> CreateNavigationTab(
    uint16_t pageIndex) override {
    return this->GetDelegate()->CreateNavigationTab(pageIndex);
  }
};

template <std::derived_from<Tab> T>
class TabWithDelegate : public virtual TabWithDelegateBase<T>,
                        public virtual std::conditional_t<
                          std::derived_from<T, TabWithCursorEvents>,
                          TabWithCursorEventsDelegate<T>,
                          void>,
                        public virtual std::conditional_t<
                          std::derived_from<T, TabWithNavigation>,
                          TabWithNavigationDelegate<T>,
                          void>,
                        private EventReceiver {
 public:
  TabWithDelegate() = delete;
  TabWithDelegate(const std::shared_ptr<T>& delegate) : mDelegate(delegate) {
    AddEventListener(delegate->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
    AddEventListener(
      delegate->evFullyReplacedEvent, this->evFullyReplacedEvent);
    AddEventListener(delegate->evPageAppendedEvent, this->evPageAppendedEvent);
    AddEventListener(
      delegate->evPageChangeRequestedEvent, this->evPageChangeRequestedEvent);
  }

 protected:
  virtual T* GetDelegate() const final override {
    return mDelegate.get();
  }

 private:
  std::shared_ptr<T> mDelegate;
};

}// namespace OpenKneeboard
