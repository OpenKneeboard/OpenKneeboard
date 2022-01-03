#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "okGamesList.h"

class wxListbook;

class okGamesList::SettingsUI final : public wxPanel {
 public:
  SettingsUI(wxWindow* parent, okGamesList* gamesList);
 private:
  wxListbook* mList = nullptr;
  okGamesList* mGamesList = nullptr;

  void OnPathSelect(wxCommandEvent& ev);
  void OnAddGameButton(wxCommandEvent& ev);
  void OnRemoveGameButton(wxCommandEvent& ev);
};
