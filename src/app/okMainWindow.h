#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>

#include <memory>

class wxBookCtrlEvent;

class okMainWindow final : public wxFrame {
 private:
	class Impl;
	std::unique_ptr<Impl> p;
 public:
	okMainWindow();
	virtual ~okMainWindow();
private:
	void OnTabChanged(wxBookCtrlEvent&);
	void OnGameEvent(wxThreadEvent&);
	void OnExit(wxCommandEvent&);
	void OnShowSettings(wxCommandEvent&);
	void OnPreviousTab(wxCommandEvent&);
	void OnNextTab(wxCommandEvent&);
	void OnPreviousPage(wxCommandEvent&);
	void OnNextPage(wxCommandEvent&);

	void UpdateSHM();
};
