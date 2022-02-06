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

#include "TabState.h"
#include "okTabCanvas.h"

using namespace OpenKneeboard;

okTab::okTab(
  wxWindow* parent,
  const DXResources& dxr,
  const std::shared_ptr<KneeboardState>& kneeboardState,
  const std::shared_ptr<TabState>& tabState)
  : wxPanel(parent) {
  auto canvas = new okTabCanvas(this, dxr, kneeboardState, tabState);

  auto buttonBox = new wxPanel(this);
  auto firstPage = new wxButton(buttonBox, wxID_ANY, _("F&irst Page"));
  auto previousPage = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
  auto nextPage = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
  firstPage->Bind(wxEVT_BUTTON, [=](auto&) { tabState->SetPageIndex(0); });
  previousPage->Bind(wxEVT_BUTTON, [=](auto&) { tabState->PreviousPage(); });
  nextPage->Bind(wxEVT_BUTTON, [=](auto&) { tabState->NextPage(); });

  auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->Add(firstPage);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(previousPage);
  buttonSizer->Add(nextPage);
  buttonBox->SetSizer(buttonSizer);

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(canvas, 1, wxEXPAND);
  sizer->Add(buttonBox, 0, wxEXPAND);
  this->SetSizerAndFit(sizer);
}

okTab::~okTab() {
}
