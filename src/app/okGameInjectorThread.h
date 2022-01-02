#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

#include <memory>
#include <vector>

#include "GameInstance.h"

class okGameInjectorThread final : public wxThread {
 public:
  okGameInjectorThread(wxEvtHandler* receiver, const std::vector<OpenKneeboard::GameInstance>&);
  ~okGameInjectorThread();

  void SetGameInstances(const std::vector<OpenKneeboard::GameInstance>&);

 protected:
  virtual ExitCode Entry() override;

 private:
  class Impl;
  std::unique_ptr<Impl> p;
};
