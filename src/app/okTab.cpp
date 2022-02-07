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
#include "okTab.h"

#include <OpenKneeboard/Tab.h>
#include <wx/wupdlock.h>

#include "TabState.h"
#include "okTabCanvas.h"

using namespace OpenKneeboard;

okTab::okTab(
  wxWindow* parent,
  const DXResources& dxr,
  const std::shared_ptr<KneeboardState>& kneeboardState,
  const std::shared_ptr<TabState>& tabState)
  : wxPanel(parent), mState(tabState) {
  InitUI(dxr, kneeboardState);

  AddEventListener(
    tabState->evPageChangedEvent, &okTab::UpdateButtonStates, this);
  UpdateButtonStates();
}

okTab::~okTab() {
}

void okTab::InitUI(
  const DXResources& dxr,
  const std::shared_ptr<KneeboardState>& kneeboard) {
  auto canvas = new okTabCanvas(this, dxr, kneeboard, mState);

  auto buttonBox = new wxPanel(this);
  mFirstPageButton = new wxButton(buttonBox, wxID_ANY, _("F&irst Page"));
  mPreviousPageButton = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
  mNextPageButton = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
  mFirstPageButton->Bind(wxEVT_BUTTON, [=](auto&) { mState->SetPageIndex(0); });
  mPreviousPageButton->Bind(
    wxEVT_BUTTON, [=](auto&) { mState->PreviousPage(); });
  mNextPageButton->Bind(wxEVT_BUTTON, [=](auto&) { mState->NextPage(); });

  auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->Add(mFirstPageButton);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(mPreviousPageButton);
  buttonSizer->Add(mNextPageButton);
  buttonBox->SetSizer(buttonSizer);

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(canvas, 1, wxEXPAND);
  sizer->Add(buttonBox, 0, wxEXPAND);
  this->SetSizerAndFit(sizer);
}

void okTab::UpdateButtonStates() {
  wxWindowUpdateLocker lock(this);
  const auto pageIndex = mState->GetPageIndex();
  mFirstPageButton->Enable(pageIndex > 0);
  mPreviousPageButton->Enable(pageIndex > 0);
  mNextPageButton->Enable(pageIndex + 1 < mState->GetPageCount());
}
