#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>

#include <memory>

class wxBookCtrlEvent;

class okMainWindow final : public wxFrame {
 public:
	okMainWindow();
	virtual ~okMainWindow();

 private:
	class Impl;
	std::unique_ptr<Impl> p;

	void OnExit(wxCommandEvent&);
	void OnGameEvent(wxThreadEvent&);
	void OnShowSettings(wxCommandEvent&);

	void OnPreviousTab(wxCommandEvent&);
	void OnNextTab(wxCommandEvent&);
	void OnPreviousPage(wxCommandEvent&);
	void OnNextPage(wxCommandEvent&);

	void OnTabChanged(wxBookCtrlEvent&);

	void UpdateTabs();
	void UpdateSHM();
};
