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

#include <OpenKneeboard/ITabView.h>

#include <vector>

namespace OpenKneeboard {

enum class ContentChangeType;
class KneeboardState;

class TabView final : public ITabView, private EventReceiver {
 public:
  TabView(KneeboardState*, const std::shared_ptr<ITab>&);
  ~TabView();

  virtual std::shared_ptr<ITab> GetRootTab() const override;

  virtual void SetPageIndex(uint16_t) override;
  virtual void NextPage() override;
  virtual void PreviousPage() override;

  virtual std::shared_ptr<ITab> GetTab() const override;
  virtual uint16_t GetPageCount() const override;
  virtual uint16_t GetPageIndex() const override;

  virtual D2D1_SIZE_U GetNativeContentSize() const override;

  virtual void PostCursorEvent(const CursorEvent&) override;

  virtual TabMode GetTabMode() const override;
  virtual bool SupportsTabMode(TabMode) const override;
  virtual bool SetTabMode(TabMode) override;

 private:
  const EventContext mEventContext;

  KneeboardState* mKneeboard;
  std::shared_ptr<ITab> mRootTab;
  uint16_t mRootTabPage;

  // For now, just navigation views, maybe more later
  std::shared_ptr<ITab> mActiveSubTab;
  uint16_t mActiveSubTabPage;

  TabMode mTabMode = TabMode::NORMAL;

  void OnTabContentChanged(ContentChangeType);
  void OnTabPageAppended();
};

}// namespace OpenKneeboard
