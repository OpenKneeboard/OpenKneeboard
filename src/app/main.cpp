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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <shims/wx.h>

#include "okMainWindow.h"

class OpenKneeboardApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    {
      OpenKneeboard::DPrintSettings::Set({
        .prefix = "OpenKneeboard",
      });

      // If already running, make the other instance active.
      //
      // There's more common ways to do this, but given we already have the SHM
      // and the SHM has a handy HWND, might as well use that.
      OpenKneeboard::SHM::Reader shm;
      if (shm) {
        auto snapshot = shm.MaybeGet();
        if (snapshot && snapshot.GetConfig().feederWindow) {
          ShowWindow(snapshot.GetConfig().feederWindow, SW_SHOWNORMAL);
          SetForegroundWindow(snapshot.GetConfig().feederWindow);
          return false;
        }
      }
    }

    wxInitAllImageHandlers();
    (new okMainWindow())->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OpenKneeboardApp);
