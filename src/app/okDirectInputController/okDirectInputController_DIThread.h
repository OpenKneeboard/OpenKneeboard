#pragma once

#include <dinput.h>
#include <winrt/base.h>

#include "okDirectInputController.h"

class okDirectInputController::DIThread final : public wxThread {
 public:
  DIThread(wxEvtHandler* receiver, const winrt::com_ptr<IDirectInput8>& di);
  virtual ~DIThread();

 protected:
  virtual ExitCode Entry() override;

 private:
  wxEvtHandler* mReceiver;
  winrt::com_ptr<IDirectInput8> mDI;
};
