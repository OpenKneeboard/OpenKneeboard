#include "okGameEvent.h"

wxDEFINE_EVENT(okID_GameEvent, okGameEvent);

okGameEvent::okGameEvent(wxEventType commandType, int id)
  : wxCommandEvent(commandType, id) {
}

okGameEvent::~okGameEvent() {
}

okGameEvent* okGameEvent::Clone() const {
  return new okGameEvent(*this);
}

void okGameEvent::SetSerializedData(const std::string& data) {
  // TODO
}

std::string okGameEvent::GetName() const {
  return mName;
}

std::string okGameEvent::GetValue() const {
  return mValue;
}
