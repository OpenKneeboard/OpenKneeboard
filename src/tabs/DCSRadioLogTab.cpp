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
  if (mMessages.empty()) {
    return 0;
  }
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

  auto metrics = dc.GetTextExtent("m");
  auto padding = metrics.GetHeight();
  auto columns = (bitmap.GetWidth() - (2 * padding)/ metrics.GetWidth());
  auto rows = (bitmap.GetHeight() - (2 * padding) / metric.GetHeight());

  if (mMessages.empty()) {
    dc.SetTextForeground(wxColor(0x99, 0x99, 0x99));
    dc.DrawText(_("[waiting for radio messages]"));
    return bitmap.ConvertToImage();
  }

  std::vector<std::string_view> lines;
  for (const auto& message: mMessages) {
    if (message.size() <= columns) {
      lines.push_back(message);
      lines.push_back({});
      continue;
    }

    std::string_view remaining(message);
    while (!remaining.empty()) {
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
      }
    }
    lines.push_back({});
  }

  auto rows = (bitmap.GetHeight() / metrics.GetHeight() - 2);
  if (lines.size() > rows) {
    lines = {lines.begin() + (lines.size() - rows), lines.end()};
  }

  dc.SetBackground(*wxWHITE_BRUSH);
  dc.Clear();

  wxPoint point {metrics.GetWidth(), metrics.GetHeight()};
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
  wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_UPDATED));
}

}// namespace OpenKneeboard
