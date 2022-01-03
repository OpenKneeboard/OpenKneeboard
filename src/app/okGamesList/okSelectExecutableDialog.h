#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class wxListView;

wxDECLARE_EVENT(okEVT_PATH_SELECTED, wxCommandEvent);

class okSelectExecutableDialog : public wxDialog {
 private:
  wxListView* mList = nullptr;

  void OnBrowseButton(wxCommandEvent&);
  void OnChooseSelectedProcess(wxCommandEvent&);

 public:
  okSelectExecutableDialog(
    wxWindow* parent,
    wxWindowID id,
    const wxString& title);
};
