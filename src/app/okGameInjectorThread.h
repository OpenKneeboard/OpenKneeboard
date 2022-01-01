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
  okGameInjectorThread(const std::vector<OpenKneeboard::GameInstance>&);
  ~okGameInjectorThread();

 protected:
  virtual ExitCode Entry() override;

 private:
  class Impl;
  std::unique_ptr<Impl> p;
};
