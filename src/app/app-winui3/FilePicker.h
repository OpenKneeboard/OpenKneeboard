// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <KnownFolders.h>
#include <ShlObj_core.h>

#include <filesystem>
#include <string>

namespace OpenKneeboard {

/** (Almost) reimplement File/Folder pickers.
 *
 * This is a workaround for
 * https://github.com/microsoft/WindowsAppSDK/issues/2731 As a bonus, it
 * supports the Saved Games known folder ID.
 */
class FilePicker final {
 public:
  FilePicker() = delete;
  FilePicker(HWND parent);

  void SetTitle(const std::wstring&);
  void SettingsIdentifier(const winrt::guid&);
  void SuggestedStartLocation(REFKNOWNFOLDERID);
  void SuggestedFileName(const std::wstring&);
  void AppendFileType(
    const std::wstring& name,
    const std::vector<std::wstring>& extensions);

  std::optional<std::filesystem::path> PickSingleFolder() const;
  std::optional<std::filesystem::path> PickSingleFile() const;
  std::optional<std::filesystem::path> PickSaveFile() const;
  std::vector<std::filesystem::path> PickMultipleFiles() const;

 private:
  void ApplySettings(const winrt::com_ptr<IFileDialog>&) const;
  std::optional<std::filesystem::path> PickSingle(FILEOPENDIALOGOPTIONS) const;
  winrt::com_ptr<IShellItem> GetInitialPath() const;
  static std::filesystem::path GetPathFromShellItem(IShellItem*);

  HWND mParent {};
  std::optional<winrt::guid> mSettingsIdentifier;
  const KNOWNFOLDERID* mSuggestedStartLocation {&FOLDERID_Documents};
  std::wstring mTitle;

  std::wstring mSuggestedFileName;

  struct FileType {
    std::wstring mName;
    std::wstring mPattern;
  };
  std::vector<FileType> mFileTypes;
};

}// namespace OpenKneeboard
