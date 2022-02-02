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

#include "okTabCanvas.h"

using namespace OpenKneeboard;

class okTab::Impl final {
 public:
  std::shared_ptr<Tab> tab;
  okTabCanvas* canvas;
};

okTab::okTab(
  wxWindow* parent,
  const std::shared_ptr<Tab>& tab,
  const DXResources& dxr)
  : wxPanel(parent), p(new Impl {.tab = tab}) {
  p->canvas = new okTabCanvas(this, tab, dxr);
  auto canvas = p->canvas;

  auto buttonBox = new wxPanel(this);
  auto firstPage = new wxButton(buttonBox, wxID_ANY, _("F&irst Page"));
  auto previousPage = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
  auto nextPage = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
  firstPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->SetPageIndex(0); });
  previousPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->PreviousPage(); });
  nextPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->NextPage(); });

  auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->Add(firstPage);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(previousPage);
  buttonSizer->Add(nextPage);
  buttonBox->SetSizer(buttonSizer);

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->canvas, 1, wxEXPAND);
  sizer->Add(buttonBox, 0, wxEXPAND);
  this->SetSizerAndFit(sizer);
}

okTab::~okTab() {
}

std::shared_ptr<Tab> okTab::GetTab() const {
  return p->tab;
}

uint16_t okTab::GetPageIndex() const {
  return p->canvas->GetPageIndex();
}

void okTab::OnCursorEvent(const CursorEvent& ev) {
  p->canvas->OnCursorEvent(ev);
}

void okTab::PreviousPage() {
  p->canvas->PreviousPage();
}

void okTab::NextPage() {
  p->canvas->NextPage();
}
