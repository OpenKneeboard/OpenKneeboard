#include "okGamesList.h"

#include <TlHelp32.h>
#include <shellapi.h>
#include <winrt/base.h>
#include <wx/gbsizer.h>
#include <wx/listbook.h>
#include <wx/listctrl.h>

#include <set>

#include "GenericGame.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

using namespace OpenKneeboard;

wxDEFINE_EVENT(okEVT_PATH_SELECTED, wxCommandEvent);

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames
    = {std::make_shared<Games::DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
    return;
  }
  LoadSettings(config);
}

void okGamesList::LoadDefaultSettings() {
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
    auto grid = new wxGridBagSizer(5, 5);

    auto bold = GetFont().MakeBold();

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Name"));
      label->SetFont(bold);
      grid->Add(label, wxGBPosition(0, 0));
      grid->Add(
        new wxStaticText(this, wxID_ANY, game.Name), wxGBPosition(0, 1));
    }

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Executable"));
      label->SetFont(bold);
      grid->Add(label, wxGBPosition(1, 0));
      grid->Add(
        new wxStaticText(this, wxID_ANY, game.Path.string()),
        wxGBPosition(1, 1));
    }

    grid->AddGrowableCol(1);

    auto s = new wxBoxSizer(wxVERTICAL);
    s->Add(grid);
    s->AddStretchSpacer();
    this->SetSizerAndFit(s);
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

std::filesystem::path GetFullPathFromPID(DWORD pid) {
  winrt::handle process {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)};
  if (!process) {
    return {};
  }

  wchar_t path[MAX_PATH];
  DWORD pathLen = MAX_PATH;
  if (!QueryFullProcessImageName(process.get(), 0, &path[0], &pathLen)) {
    return {};
  }

  return std::wstring(&path[0], pathLen);
}

int wxCALLBACK compareProcessItems(wxIntPtr a, wxIntPtr b, wxIntPtr sortData) {
  auto list = reinterpret_cast<wxListView*>(sortData);

  auto at = list->GetItemText(a).Lower();
  auto bt = list->GetItemText(b).Lower();

  if (at == bt) {
    return 0;
  }
  return (at < bt) ? -1 : 1;
}

class okSelectProcessDialog : public wxDialog {
 public:
  okSelectProcessDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title) {
    auto list = new wxListView(this, wxID_ANY);
    list->AppendColumn(_("Name"));
    list->AppendColumn(_("Path"));
    auto images = new wxImageList(16, 16);
    list->SetImageList(images, wxIMAGE_LIST_SMALL);
    list->AssignImageList(images, wxIMAGE_LIST_NORMAL);

    winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    PROCESSENTRY32 process;
    process.dwSize = sizeof(process);
    Process32First(snapshot.get(), &process);
    std::set<std::filesystem::path> seen;
    do {
      auto path = GetFullPathFromPID(process.th32ProcessID);
      if (path.empty()) {
        continue;
      }
      if (seen.contains(path)) {
        continue;
      }
      seen.emplace(path);

      auto row = list->GetItemCount();

      auto icon = GetIconFromExecutable(path);
      if (icon.IsOk()) {
        auto idx = images->Add(icon);
        list->InsertItem(row, path.stem().wstring(), idx);
      } else {
        list->InsertItem(row, path.stem().wstring(), -1);
      }
      list->SetItem(row, 1, path.wstring());
      list->SetItemData(row, row);
    } while (Process32Next(snapshot.get(), &process));

    list->SetColumnWidth(0, wxLIST_AUTOSIZE);
    list->SetColumnWidth(1, wxLIST_AUTOSIZE);
    list->SortItems(compareProcessItems, reinterpret_cast<wxIntPtr>(list));

    auto s = new wxBoxSizer(wxVERTICAL);
    s->Add(list);
    this->SetSizerAndFit(s);

    list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [=](wxListEvent& ev) {
      auto item = ev.GetItem();
      auto path = list->GetItemText(item.GetId(), 1);
      auto ce = new wxCommandEvent(okEVT_PATH_SELECTED);
      ce->SetEventObject(this);
      ce->SetString(path);
      wxQueueEvent(this, ce);
    });
  }
};

}// namespace

class okGamesList::SettingsUI final : public wxPanel {
 private:
  wxListbook* mList = nullptr;
  okGamesList* mGamesList = nullptr;

  void OnPathSelect(wxCommandEvent& ev) {
    auto path = std::filesystem::path(ev.GetString().ToStdWstring());
    int imageIndex = -1;
    auto ico = GetIconFromExecutable(path);
    auto imageList = mList->GetImageList();
    if (ico.IsOk()) {
      imageIndex = imageList->Add(ico);
    }
    for (const auto& game: mGamesList->mGames) {
      if (!game->MatchesPath(path)) {
        continue;
      }
      GameInstance instance {
        .Name = game->GetUserFriendlyName(path).ToStdString(),
        .Path = path,
        .Game = game};
      mGamesList->mInstances.push_back(instance);
      mList->AddPage(
        new okGameInstanceSettings(mList, instance),
        instance.Name,
        false,
        imageIndex);
    }
    auto dialog = dynamic_cast<wxDialog*>(ev.GetEventObject());
    if (!dialog) {
      dprintf("No dialog in {}", __FUNCTION__);
      return;
    }
    wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
    dialog->Close();
  }

  void OnAddGame(wxCommandEvent& ev) {
    auto dialog = std::make_unique<okSelectProcessDialog>(
      nullptr, wxID_ANY, _("Select Game"));
    dialog->Bind(okEVT_PATH_SELECTED, &SettingsUI::OnPathSelect, this);
    dialog->ShowModal();
  }

 public:
  SettingsUI(wxWindow* parent, okGamesList* gamesList)
    : wxPanel(parent, wxID_ANY), mGamesList(gamesList) {
    this->SetLabel(_("Games"));
    mList = new wxListbook(this, wxID_ANY);
    mList->SetWindowStyleFlag(wxLB_LEFT);
    auto imageList = new wxImageList(32, 32);
    mList->AssignImageList(imageList);

    for (const auto& game: mGamesList->mInstances) {
      int imageIndex = -1;
      auto ico = GetIconFromExecutable(game.Path);
      if (ico.IsOk()) {
        imageIndex = imageList->Add(ico);
      }
      mList->AddPage(
        new okGameInstanceSettings(mList, game), game.Name, false, imageIndex);
    }

    auto add = new wxButton(this, wxID_ANY, _("&Add Game"));
    add->Bind(wxEVT_BUTTON, &SettingsUI::OnAddGame, this);
    auto remove = new wxButton(this, wxID_ANY, _("&Remove Game"));

    auto buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->Add(add);
    buttons->AddStretchSpacer();
    buttons->Add(remove);

    auto s = new wxBoxSizer(wxVERTICAL);
    s->Add(mList, 1, wxEXPAND | wxFIXED_MINSIZE, 5);
    s->Add(buttons, 0, wxEXPAND, 5);
    this->SetSizerAndFit(s);
  }
};

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, this);
  ret->Bind(okEVT_SETTINGS_CHANGED, [=](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
  return ret;
}

nlohmann::json okGamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game.ToJson());
  }
  return { { "Configured", games }};
}

void okGamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured"); 

  for (const auto& game: list) {
    mInstances.push_back(GameInstance::FromJson(game, mGames));
  }
}
