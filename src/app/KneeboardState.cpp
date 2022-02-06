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
#include "KneeboardState.h"

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/config.h>

#include "TabState.h"
#include "okEvents.h"

namespace OpenKneeboard {

KneeboardState::KneeboardState() {
	evCursorEvent.AddHandler(this, std::bind_front(&KneeboardState::OnCursorEvent, this));
}

KneeboardState::~KneeboardState() {
}

std::vector<std::shared_ptr<TabState>> KneeboardState::GetTabs() const {
	return mTabs;
}

void KneeboardState::SetTabs(
	const std::vector<std::shared_ptr<TabState>>& tabs) {
	auto oldTab = mCurrentTab;

	mTabs = tabs;

	auto it = std::find(tabs.begin(), tabs.end(), mCurrentTab);
	if (it == tabs.end()) {
		mCurrentTab = tabs.empty() ? nullptr : tabs.front();
	}
	UpdateLayout();
}

void KneeboardState::InsertTab(
	uint8_t index,
	const std::shared_ptr<TabState>& tab) {
	mTabs.insert(mTabs.begin() + index, tab);
	if (!mCurrentTab) {
		mCurrentTab = tab;
		UpdateLayout();
	}
}

void KneeboardState::AppendTab(const std::shared_ptr<TabState>& tab) {
	mTabs.push_back(tab);
	if (!mCurrentTab) {
		mCurrentTab = tab;
		UpdateLayout();
	}
}

void KneeboardState::RemoveTab(uint8_t index) {
	if (index >= mTabs.size()) {
		return;
	}

	const bool wasSelected = (mTabs.at(index) == mCurrentTab);
	mTabs.erase(mTabs.begin() + index);

	if (!wasSelected) {
		return;
	}

	if (index < mTabs.size()) {
		mCurrentTab = mTabs.at(index);
	} else if (!mTabs.empty()) {
		mCurrentTab = mTabs.front();
	} else {
		mCurrentTab = {};
	}

	UpdateLayout();
}

uint8_t KneeboardState::GetTabIndex() const {
	auto it = std::find(mTabs.begin(), mTabs.end(), mCurrentTab);
	if (it == mTabs.end()) {
		return 0;
	}
	return it - mTabs.begin();
}

void KneeboardState::SetTabIndex(uint8_t index) {
	if (index >= mTabs.size()) {
		return;
	}
	if (mCurrentTab) {
		mCurrentTab->PostCursorEvent({});
	}
	mCurrentTab = mTabs.at(index);
	UpdateLayout();
}

std::shared_ptr<TabState> KneeboardState::GetCurrentTab() const {
	return mCurrentTab;
}

void KneeboardState::NextPage() {
	if (mCurrentTab) {
		mCurrentTab->NextPage();
		UpdateLayout();
	}
}

void KneeboardState::PreviousPage() {
	if (mCurrentTab) {
		mCurrentTab->PreviousPage();
		UpdateLayout();
	}
}

void KneeboardState::UpdateLayout() {
	const auto totalHeightRatio = 1 + (HeaderPercent / 100.0f);

  mContentNativeSize = { 768, 1024 };
  auto tab = GetCurrentTab();
  if (tab) {
    mContentNativeSize = tab->GetNativeContentSize();
  }

	const auto scaleX
		= static_cast<float>(TextureWidth) / mContentNativeSize.width;
	const auto scaleY = static_cast<float>(TextureHeight)
		/ (totalHeightRatio * mContentNativeSize.height);
	const auto scale = std::min(scaleX, scaleY);
	const D2D1_SIZE_F contentRenderSize {
		mContentNativeSize.width * scale,
		mContentNativeSize.height * scale,
	};
	const D2D1_SIZE_F headerRenderSize {
		static_cast<FLOAT>(contentRenderSize.width),
		contentRenderSize.height * (HeaderPercent / 100.0f),
	};

	mCanvasSize = {
		static_cast<UINT>(contentRenderSize.width),
		static_cast<UINT>(contentRenderSize.height + headerRenderSize.height),
	};
	mHeaderRenderRect = {
		.left = 0,
		.top = 0,
		.right = headerRenderSize.width,
		.bottom = headerRenderSize.height,
	};
	mContentRenderRect = {
		.left = 0,
		.top = mHeaderRenderRect.bottom,
		.right = contentRenderSize.width,
		.bottom = mHeaderRenderRect.bottom + contentRenderSize.height,
	};
}

const D2D1_SIZE_U& KneeboardState::GetCanvasSize() const {
	return mCanvasSize;
}

const D2D1_SIZE_U& KneeboardState::GetContentNativeSize() const {
	return mContentNativeSize;
}

const D2D1_RECT_F& KneeboardState::GetHeaderRenderRect() const {
	return mHeaderRenderRect;
}

const D2D1_RECT_F& KneeboardState::GetContentRenderRect() const {
	return mContentRenderRect;
}

void KneeboardState::OnCursorEvent(const CursorEvent& ev) {
	mCursorPoint = {ev.mX, ev.mY};
	mHaveCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;

	if (mCurrentTab) {
		mCurrentTab->PostCursorEvent(ev);
	}
}

void KneeboardState::PostGameEvent(const GameEvent& ev) {
	for (auto tab: mTabs) {
		tab->GetRootTab()->PostGameEvent(ev);
	}
}

bool KneeboardState::HaveCursor() const {
	return mHaveCursor;
}

D2D1_POINT_2F KneeboardState::GetCursorPoint() const {
	return mCursorPoint;
}

}// namespace OpenKneeboard
