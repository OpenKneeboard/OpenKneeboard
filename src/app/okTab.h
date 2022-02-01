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

#include <dxgi1_2.h>

#include <memory>

#include "shims/winrt.h"
#include <shims/wx.h>

namespace OpenKneeboard {
class Tab;
}

class okTab final : public wxPanel {
 public:
  okTab(
    wxWindow* parent,
    const std::shared_ptr<OpenKneeboard::Tab>&,
    const winrt::com_ptr<IDXGIDevice2>&);
  virtual ~okTab();

  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;
  uint16_t GetPageIndex() const;

  void NextPage();
  void PreviousPage();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void EmitPageChanged();
};
