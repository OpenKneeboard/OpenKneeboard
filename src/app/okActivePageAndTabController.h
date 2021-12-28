#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

#include <json.hpp>

/** Controller to change what tab and page are active.
 *
 * Expected to raise events:
 * - okEVT_SETTINGS_CHANGED
 * - okEVT_{PREVIOUS,NEXT}_{PAGE,TAB}
 */
class okActivePageAndTabController : public wxEvtHandler {
  public:
    okActivePageAndTabController();
    virtual ~okActivePageAndTabController();

    virtual wxString GetTitle() const = 0;

    virtual wxWindow* GetSettingsUI(wxWindow* parent);
    virtual nlohmann::json GetSettings() const;
};
