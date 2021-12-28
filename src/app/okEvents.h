#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

wxDECLARE_EVENT(okEVT_PREVIOUS_TAB, wxCommandEvent);
wxDECLARE_EVENT(okEVT_NEXT_TAB, wxCommandEvent);
wxDECLARE_EVENT(okEVT_PREVIOUS_PAGE, wxCommandEvent);
wxDECLARE_EVENT(okEVT_NEXT_PAGE, wxCommandEvent);

wxDECLARE_EVENT(okEVT_SETTINGS_CHANGED, wxCommandEvent);
