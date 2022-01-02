#include "okSelectExecutableDialog.h"

#include <OpenKneeboard/dprint.h>
#include <ShlObj.h>
#include <TlHelp32.h>
#include <winrt/base.h>
#include <wx/listctrl.h>

#include <set>

#include "GetIconFromExecutable.h"
#include "okEvents.h"

wxDEFINE_EVENT(okEVT_PATH_SELECTED, wxCommandEvent);

using namespace OpenKneeboard;

static int wxCALLBACK
compareProcessItems(wxIntPtr a, wxIntPtr b, wxIntPtr sortData) {
  auto list = reinterpret_cast<wxListView*>(sortData);

  auto at = list->GetItemText(a).Lower();
  auto bt = list->GetItemText(b).Lower();

  if (at == bt) {
    return 0;
  }
  return (at < bt) ? -1 : 1;
}

static std::filesystem::path GetFullPathFromPID(DWORD pid) {
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

void okSelectExecutableDialog::OnBrowseButton(wxCommandEvent&) {
  wxFileDialog dialog(this, _("Choose Game"));
  dialog.SetWildcard(_("Executables (*.exe)|*.exe"));
  dialog.SetWindowStyle(wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  wchar_t* buffer = nullptr;
  if (
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, NULL, NULL, &buffer) == S_OK
    && buffer) {
    dialog.SetDirectory(wxString(buffer));
  }

  if (dialog.ShowModal() == wxID_CANCEL) {
    return;
  }

  auto path = std::filesystem::canonical(dialog.GetPath().ToStdWstring());
  dprintf("Raw path: {}", dialog.GetPath().ToStdString());
  if (!std::filesystem::is_regular_file(path)) {
    dprintf("Asked to use '{}', but is not a file", path.string());
    return;
  }
  if (path.extension() != ".exe") {
    dprintf(
      "Asked to use '{}', but extension '{}' is not '.exe'",
      path.string(),
      path.extension().string());
    return;
  }

  wxCommandEvent ev(okEVT_PATH_SELECTED);
  ev.SetEventObject(this);
  ev.SetString(path.wstring());
  wxQueueEvent(this, ev.Clone());
}

void okSelectExecutableDialog::OnChooseSelectedProcess(wxCommandEvent&) {
  auto index = mList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (index == -1) {
    return;
  }

  auto path = mList->GetItemText(index, 1);
  auto ce = new wxCommandEvent(okEVT_PATH_SELECTED);
  ce->SetEventObject(this);
  ce->SetString(path);
  wxQueueEvent(this, ce);
}

okSelectExecutableDialog::okSelectExecutableDialog(
  wxWindow* parent,
  wxWindowID id,
  const wxString& title)
  : wxDialog(parent, id, title) {
  mList = new wxListView(this, wxID_ANY);
  mList->SetWindowStyle(wxLC_REPORT | wxLC_SINGLE_SEL);
  mList->AppendColumn(_("Name"));
  mList->AppendColumn(_("Path"));
  auto images = new wxImageList(16, 16);
  mList->SetImageList(images, wxIMAGE_LIST_SMALL);
  mList->AssignImageList(images, wxIMAGE_LIST_NORMAL);

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

    auto row = mList->GetItemCount();

    auto icon = GetIconFromExecutable(path);
    if (icon.IsOk()) {
      auto idx = images->Add(icon);
      mList->InsertItem(row, path.stem().wstring(), idx);
    } else {
      mList->InsertItem(row, path.stem().wstring(), -1);
    }
    mList->SetItem(row, 1, path.wstring());
    mList->SetItemData(row, row);
  } while (Process32Next(snapshot.get(), &process));

  mList->SetColumnWidth(0, wxLIST_AUTOSIZE);
  mList->SetColumnWidth(1, wxLIST_AUTOSIZE);
  mList->SortItems(compareProcessItems, reinterpret_cast<wxIntPtr>(mList));

  mList->Bind(
    wxEVT_LIST_ITEM_ACTIVATED,
    &okSelectExecutableDialog::OnChooseSelectedProcess,
    this);

  auto buttons = new wxBoxSizer(wxHORIZONTAL);

  auto browse = new wxButton(this, wxID_ANY, _("&Browse..."));
  buttons->Add(browse);
  browse->Bind(wxEVT_BUTTON, &okSelectExecutableDialog::OnBrowseButton, this);

  buttons->AddStretchSpacer();

  auto ok = new wxButton(this, wxID_ANY, _("&OK"));
  buttons->Add(ok);
  ok->Bind(
    wxEVT_BUTTON, &okSelectExecutableDialog::OnChooseSelectedProcess, this);
  ok->Disable();
  mList->Bind(wxEVT_LIST_ITEM_SELECTED, [=](auto&) { ok->Enable(); });

  buttons->Add(new wxButton(this, wxID_CANCEL, _("&Cancel")));

  auto s = new wxBoxSizer(wxVERTICAL);
  s->Add(mList, 1);
  s->AddSpacer(5);
  s->Add(buttons, 0, wxEXPAND);
  this->SetSizerAndFit(s);
}
