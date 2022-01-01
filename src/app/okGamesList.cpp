#include "okGamesList.h"

#include <shellapi.h>
#include <winrt/base.h>
#include <wx/listbook.h>

#include "OpenKneeboard/Games/DCSWorld.h"

using namespace OpenKneeboard;

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames = {std::make_shared<Games::DCSWorld>()};

  if (!config.is_null()) {
    // TODO
    return;
  }

  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back({
        .Name = game->GetUserFriendlyName(path).ToStdString(),
        .Path = path,
        .Game = game,
      });
    }
  }
}

okGamesList::~okGamesList() {
}

namespace {
class okGameInstanceSettings : public wxPanel {
 public:
  okGameInstanceSettings(wxWindow* parent, const GameInstance& game)
    : wxPanel(parent, wxID_ANY) {
    auto s = new wxGridSizer(2, 5, 5);

    auto bold = GetFont().MakeBold();

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Name"));
      label->SetFont(bold);
      s->Add(label);
      s->Add(new wxStaticText(this, wxID_ANY, game.Name), 1);
    }

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Executable"));
      label->SetFont(bold);
      s->Add(label);
      s->Add(new wxStaticText(this, wxID_ANY, game.Path.string()), 1);
    }

    s->AddStretchSpacer();
    s->AddStretchSpacer();

    SetSizerAndFit(s);
  }
};

wxIcon GetIconFromExecutable(const std::filesystem::path& path) {
  auto wpath = path.wstring();
  HICON handle = ExtractIcon(GetModuleHandle(NULL), wpath.c_str(), 0);
  if (!handle) {
    return {};
  }

  wxIcon icon;
  if (icon.CreateFromHICON(handle)) {
    // `icon` now owns the handle
    return icon;
  }

  DestroyIcon(handle);

  return {};
}

}// namespace

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto list = new wxListbook(parent, wxID_ANY);
  list->SetLabel(_("Games"));
  list->SetWindowStyleFlag(wxLB_LEFT);
  auto imageList = new wxImageList(32, 32);
  list->SetImageList(imageList);

  for (const auto& game: mInstances) {
    int imageIndex = -1;
    auto ico = GetIconFromExecutable(game.Path);
    if (ico.IsOk()) {
      imageIndex = imageList->Add(ico);
    }
    list->AddPage(new okGameInstanceSettings(list, game), game.Name, false, imageIndex);
  }

  return list;
}

nlohmann::json okGamesList::GetSettings() const {
  return {};
}
