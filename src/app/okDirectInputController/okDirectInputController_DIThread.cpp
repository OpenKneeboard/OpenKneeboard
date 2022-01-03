#include "okDirectInputController_DIThread.h"

#include "GetDirectInputDevices.h"
#include "okDirectInputController_DIButtonEvent.h"
#include "okDirectInputController_DIButtonListener.h"

using namespace OpenKneeboard;

okDirectInputController::DIThread::DIThread(
  wxEvtHandler* receiver,
  const winrt::com_ptr<IDirectInput8>& di)
  : wxThread(wxTHREAD_JOINABLE), mReceiver(receiver) {
}

okDirectInputController::DIThread::~DIThread() {
}

wxThread::ExitCode okDirectInputController::DIThread::Entry() {
  auto listener = DIButtonListener(mDI, GetDirectInputDevices(mDI));
  while (!this->TestDestroy()) {
    DIButtonEvent bi = listener.Poll();
    if (!bi) {
      continue;
    }
    wxThreadEvent ev(okEVT_DI_BUTTON);
    ev.SetPayload(bi);
    wxQueueEvent(mReceiver, ev.Clone());
  }

  return ExitCode(0);
}
