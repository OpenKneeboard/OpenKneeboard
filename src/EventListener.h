#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

wxDECLARE_EVENT(YAVRK_EVENT, wxCommandEvent);

class EventListener final : public wxThread {
 private:
  wxFrame* mParent;

 public:
  EventListener(wxFrame* parent);
  ~EventListener();

 protected:
  virtual ExitCode Entry() override;
};
