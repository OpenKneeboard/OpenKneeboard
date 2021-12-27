#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

namespace OpenKneeboard {

void RenderError(const wxSize& clientSize, wxDC& dc, const wxString& message);

}
