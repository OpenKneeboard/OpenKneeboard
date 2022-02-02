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
#include <shims/winrt.h>
#include <shims/wx.h>
#include <wx/image.h>

#include <memory>

namespace OpenKneeboard {
struct CursorEvent;
class Tab;
}// namespace OpenKneeboard

class okTabCanvas final : public wxPanel {
 public:
  okTabCanvas(
    wxWindow* parent,
    const std::shared_ptr<OpenKneeboard::Tab>&,
    const winrt::com_ptr<IDXGIDevice2>&);
  virtual ~okTabCanvas();

  void OnSize(wxSizeEvent& ev);
  void OnPaint(wxPaintEvent& ev);
  void OnEraseBackground(wxEraseEvent& ev);
  void OnCursorEvent(const OpenKneeboard::CursorEvent&);

  uint16_t GetPageIndex() const;
  void SetPageIndex(uint16_t index);
  void NextPage();
  void PreviousPage();
  std::shared_ptr<OpenKneeboard::Tab> GetTab() const;

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void OnTabFullyReplaced(wxCommandEvent&);
  void OnTabPageModified(wxCommandEvent&);
  void OnTabPageAppended(wxCommandEvent&);
  void OnPixelsChanged();
};
