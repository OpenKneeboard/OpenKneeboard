#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "GameInstance.h"

class okGameInstanceSettings : public wxPanel {
 private:
  OpenKneeboard::GameInstance mGame;

 public:
  okGameInstanceSettings(
    wxWindow* parent,
    const OpenKneeboard::GameInstance& game);
  OpenKneeboard::GameInstance GetGameInstance() const;
};
