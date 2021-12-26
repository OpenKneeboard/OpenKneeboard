#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

wxDECLARE_EVENT(OPENKNEEBOARD_EVENT, wxCommandEvent);

class GameEventNamedPipeListener final : public wxThread {
 private:
  wxFrame* mParent;

 public:
  GameEventNamedPipeListener(wxFrame* parent);
  ~GameEventNamedPipeListener();

 protected:
  virtual ExitCode Entry() override;
};
