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

#include <d2d1.h>
#include <shims/winrt.h>

#include <memory>
#include <string>

#include "okConfigurableComponent.h"

namespace OpenKneeboard {
struct GameEvent;
struct CursorEvent;
struct DXResources;

class Tab : public okConfigurableComponent {
 public:
  Tab() = delete;
  Tab(const DXResources&, const wxString& title);
  virtual ~Tab();

  std::string GetTitle() const;

  virtual void Reload();
  virtual void OnGameEvent(const GameEvent&);

  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;

  virtual uint16_t GetPageCount() const = 0;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) = 0;
  void RenderPage(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect);

  virtual void OnCursorEvent(const CursorEvent&, uint16_t pageIndex);

 protected:
  void ClearDrawings();

  virtual void RenderPageContent(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect)
    = 0;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
