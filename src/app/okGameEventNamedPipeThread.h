#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

wxDECLARE_EVENT(OPENKNEEBOARD_EVENT, wxCommandEvent);

class okGameEventNamedPipeThread final : public wxThread {
 private:
  wxFrame* mParent;

 public:
  okGameEventNamedPipeThread(wxFrame* parent);
  ~okGameEventNamedPipeThread();

 protected:
  virtual ExitCode Entry() override;
};
