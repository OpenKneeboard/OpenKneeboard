#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

wxDECLARE_EVENT(okEVT_GAME_EVENT, wxThreadEvent);

class okGameEventNamedPipeThread final : public wxThread {
 private:
  wxFrame* mParent;

 public:
  struct Payload {
    std::string Name;
    std::string Value;
  };
  okGameEventNamedPipeThread(wxFrame* parent);
  ~okGameEventNamedPipeThread();

 protected:
  virtual ExitCode Entry() override;
};
