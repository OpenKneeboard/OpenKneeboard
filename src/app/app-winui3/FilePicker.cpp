// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "FilePicker.h"

#include <wil/resource.h>

#include <algorithm>
#include <ranges>
#include <string>

namespace OpenKneeboard {

FilePicker::FilePicker(HWND parent) : mParent(parent) {}

void FilePicker::SettingsIdentifier(const winrt::guid& guid) {
  mSettingsIdentifier = guid;
}

void FilePicker::SuggestedStartLocation(REFKNOWNFOLDERID folderID) {
  mSuggestedStartLocation = &folderID;
}

void FilePicker::SuggestedFileName(const std::wstring& fileName) {
  mSuggestedFileName = fileName;
}

void FilePicker::AppendFileType(
  const std::wstring& name,
  const std::vector<std::wstring>& extensions) {
  using namespace std::literals::string_literals;

  // clang-format off
  const auto pattern = extensions
    | std::views::transform([](const auto& ext) { return L"*"s + ext; })
    | std::views::join_with(L";"s)
    | std::ranges::to<std::wstring>();
  // clang-format on

  mFileTypes.push_back({
    .mName = std::wstring {name},
    .mPattern = pattern,
  });
}

std::optional<std::filesystem::path> FilePicker::PickSingleFolder() const {
  return PickSingle(FOS_PICKFOLDERS | FOS_FILEMUSTEXIST);
}

std::optional<std::filesystem::path> FilePicker::PickSingleFile() const {
  return PickSingle(FOS_FILEMUSTEXIST | FOS_FILEMUSTEXIST);
}

void FilePicker::ApplySettings(
  const winrt::com_ptr<IFileDialog>& picker) const {
  const auto initialFolder = this->GetInitialPath();
  picker->SetDefaultFolder(initialFolder.get());

  if (!mSuggestedFileName.empty()) {
    picker->SetFileName(mSuggestedFileName.c_str());
  }

  if (!mTitle.empty()) {
    picker->SetTitle(mTitle.c_str());
  }

  // Keep alive until after we're done with the dialog
  std::vector<COMDLG_FILTERSPEC> fileTypes;
  if (!mFileTypes.empty()) {
    for (const auto& type: mFileTypes) {
      fileTypes.push_back({type.mName.c_str(), type.mPattern.c_str()});
    }
    picker->SetFileTypes(static_cast<UINT>(fileTypes.size()), fileTypes.data());
  }
}

std::optional<std::filesystem::path> FilePicker::PickSingle(
  FILEOPENDIALOGOPTIONS options) const {
  auto picker =
    winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog, CLSCTX_ALL);

  // Not part of ApplySettings() as it Should be the first thing called
  if (mSettingsIdentifier) {
    picker->SetClientGuid(*mSettingsIdentifier);
  }

  FILEOPENDIALOGOPTIONS allOpts;
  picker->GetOptions(&allOpts);
  allOpts |= options | FOS_FORCEFILESYSTEM;
  picker->SetOptions(allOpts);
  ApplySettings(picker);

  if (picker->Show(mParent) != S_OK) {
    return {};
  }

  winrt::com_ptr<IShellItem> shellItem;
  winrt::check_hresult(picker->GetResult(shellItem.put()));
  return GetPathFromShellItem(shellItem.get());
}

std::optional<std::filesystem::path> FilePicker::PickSaveFile() const {
  auto picker =
    winrt::create_instance<IFileSaveDialog>(CLSID_FileSaveDialog, CLSCTX_ALL);

  // Not part of ApplySettings() as it Should be the first thing called
  if (mSettingsIdentifier) {
    picker->SetClientGuid(*mSettingsIdentifier);
  }

  FILEOPENDIALOGOPTIONS allOpts;
  picker->GetOptions(&allOpts);
  allOpts |= FOS_FORCEFILESYSTEM;
  picker->SetOptions(allOpts);
  ApplySettings(picker);

  if (picker->Show(mParent) != S_OK) {
    return {};
  }

  winrt::com_ptr<IShellItem> shellItem;
  winrt::check_hresult(picker->GetResult(shellItem.put()));
  return GetPathFromShellItem(shellItem.get());
}

std::filesystem::path FilePicker::GetPathFromShellItem(IShellItem* shellItem) {
  wil::unique_cotaskmem_string pathStr;
  winrt::check_hresult(
    shellItem->GetDisplayName(SIGDN_FILESYSPATH, std::out_ptr(pathStr)));
  const std::filesystem::path path(std::wstring_view(pathStr.get()));
  if (std::filesystem::exists(path)) {
    return std::filesystem::canonical(path);
  } else {
    return std::filesystem::weakly_canonical(path);
  }
}

void FilePicker::SetTitle(const std::wstring& title) { mTitle = title; }

std::vector<std::filesystem::path> FilePicker::PickMultipleFiles() const {
  auto picker =
    winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog, CLSCTX_ALL);

  // Not part of ApplySettings() as it Should be the first thing called
  if (mSettingsIdentifier) {
    picker->SetClientGuid(*mSettingsIdentifier);
  }

  picker->SetOptions(
    FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM
    | FOS_ALLOWMULTISELECT);
  ApplySettings(picker);

  if (picker->Show(mParent) != S_OK) {
    return {};
  }

  winrt::com_ptr<IShellItemArray> items;
  winrt::check_hresult(picker->GetResults(items.put()));
  if (!items) {
    return {};
  }

  DWORD count {};
  winrt::check_hresult(items->GetCount(&count));

  std::vector<std::filesystem::path> ret;
  for (DWORD i = 0; i < count; ++i) {
    winrt::com_ptr<IShellItem> it;
    winrt::check_hresult(items->GetItemAt(i, it.put()));
    ret.push_back(GetPathFromShellItem(it.get()));
  }

  return ret;
}

winrt::com_ptr<IShellItem> FilePicker::GetInitialPath() const {
  winrt::com_ptr<IShellItem> ret;
  winrt::check_hresult(SHGetKnownFolderItem(
    *mSuggestedStartLocation, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(ret.put())));
  return ret;
}

}// namespace OpenKneeboard
