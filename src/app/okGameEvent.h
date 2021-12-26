#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class okGameEvent;
wxDECLARE_EVENT(okEVT_GAME_EVENT, okGameEvent);

class okGameEvent final : public wxCommandEvent {
  public:
    okGameEvent(wxEventType commandType = okEVT_GAME_EVENT, int id = 0);
    virtual ~okGameEvent();

    okGameEvent* Clone() const override;

    // %08x!%s!%08x!%s!
    void SetSerializedData(const std::string& data);

    std::string GetName() const;
    std::string GetValue() const;
  private:
    std::string mName;
    std::string mValue;
};

typedef void (wxEvtHandler::*okGameEventFunction)(okGameEvent &);

#define okGameEventHandler(func) wxEVENT_HANDLER_CAST(okGameEventFunction, func)

#define EVT_OK_GAMEEVENT(id, func) \
 	wx__DECLARE_EVT1(okEVT_GAME_EVENT, id, MyFooEventHandler(func))
