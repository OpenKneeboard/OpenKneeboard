#include "OpenKneeboard/DCSRadioLogTab.h"

#include "OpenKneeboard/Games/DCSWorld.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSRadioLogTab::DCSRadioLogTab() : DCSTab(_("Radio Log")) {
}

DCSRadioLogTab::~DCSRadioLogTab() {
}

void DCSRadioLogTab::Reload() {
}

uint16_t DCSRadioLogTab::GetPageCount() const {
  return 1;
}

wxImage DCSRadioLogTab::RenderPage(uint16_t index) {
  if (index != 0) {
    return {};
  }

  wxBitmap bitmap(768, 1024);
  wxMemoryDC dc(bitmap);

  dc.SetFont(
    wxFont(16, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
  dc.SetBackground(*wxWHITE_BRUSH);
  dc.Clear();

  auto metrics = dc.GetTextExtent("m");
  auto padding = metrics.GetHeight();
  const auto columns = (bitmap.GetWidth() - (2 * padding)) / metrics.GetWidth();
  const auto rows = (bitmap.GetHeight() - (2 * padding)) / metrics.GetHeight();

  if (mMessages.empty()) {
    dc.SetTextForeground(wxColor(0x55, 0x55, 0x55));
    dc.DrawText(_("[waiting for radio messages]"), wxPoint(padding, padding));
    return bitmap.ConvertToImage();
  }

  std::vector<std::string_view> lines;
  for (const auto& message: mMessages) {
    std::string_view remaining(message);
    while (!remaining.empty()) {
      if (remaining.size() <= columns) {
        lines.push_back(remaining);
        break;
      }
      auto space = remaining.find_last_of(" ", columns);
      if (space == remaining.npos) {
        lines.push_back(remaining.substr(0, columns));
        if (remaining.size() > columns) {
          remaining = remaining.substr(columns);
          continue;
        }
        break;
      }

      lines.push_back(remaining.substr(0, space));
      if (remaining.size() > space + 1) {
        remaining = remaining.substr(space + 1);
      } else {
        break;
      }
    }
    lines.push_back({});
  }

  if (lines.size() > rows) {
    lines = {lines.begin() + (lines.size() - rows), lines.end()};
  }

  wxPoint point {padding, padding};
  auto y = metrics.GetHeight();
  for (const auto& line: lines) {
    dc.DrawText(wxString(line.data(), line.size()), point);
    point.y += metrics.GetHeight();
  }

  return bitmap.ConvertToImage();
}

const char* DCSRadioLogTab::GetGameEventName() const {
  return DCS::EVT_RADIO_MESSAGE;
}

void DCSRadioLogTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  const std::string& value) {
  mMessages.push_back(value);
}

}// namespace OpenKneeboard
