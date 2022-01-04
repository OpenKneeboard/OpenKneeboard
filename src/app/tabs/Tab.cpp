#include "OpenKneeboard/Tab.h"

#include "okEvents.h"

namespace OpenKneeboard {

class Tab::Impl final {
 public:
  std::string Title;
};

Tab::Tab(const wxString& title) : p(new Impl {.Title = title.ToStdString()}) {
}

Tab::~Tab() {
}

std::string Tab::GetTitle() const {
  return p->Title;
}

void Tab::Reload() {
}

void Tab::OnGameEvent(const GameEvent&) {
}

wxWindow* Tab::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json Tab::GetSettings() const {
  return {};
}

}// namespace OpenKneeboard
