#include "okDirectInputController_DIButtonEvent.h"

wxDEFINE_EVENT(okEVT_DI_BUTTON, wxThreadEvent);

okDirectInputController::DIButtonEvent::operator bool() const {
  return this->valid;
}
