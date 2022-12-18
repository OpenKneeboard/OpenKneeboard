/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <OpenKneeboard/ITabWithSettings.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <OpenKneeboard/TabBase.h>
#include <OpenKneeboard/handles.h>

namespace OpenKneeboard {

class WindowCaptureTab final
  : public TabBase,
    public ITabWithSettings,
    public PageSourceWithDelegates,
    public std::enable_shared_from_this<WindowCaptureTab> {
 public:
  struct WindowSpecification {
    std::filesystem::path mExecutable;
    std::string mWindowClass;
    std::string mTitle;
  };
  struct MatchSpecification : public WindowSpecification {
    enum class TitleMatchKind {
      Ignore,
      Exact,
      Glob,
    };

    TitleMatchKind mMatchTitle {TitleMatchKind::Exact};
    bool mMatchWindowClass {true};
    bool mMatchExecutable {true};
  };

  WindowCaptureTab() = delete;
  static std::shared_ptr<WindowCaptureTab>
  Create(const DXResources&, KneeboardState*, const MatchSpecification&);
  static std::shared_ptr<WindowCaptureTab> Create(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title,
    const nlohmann::json& settings);
  virtual ~WindowCaptureTab();
  virtual utf8_string GetGlyph() const override;

  virtual void Reload() final override;
  virtual nlohmann::json GetSettings() const override;

  MatchSpecification GetMatchSpecification() const;
  void SetMatchSpecification(const MatchSpecification&);

  static std::unordered_map<HWND, WindowSpecification> GetTopLevelWindows();
  static std::optional<WindowSpecification> GetWindowSpecification(HWND);

 private:
  WindowCaptureTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title,
    const MatchSpecification&);
  winrt::fire_and_forget TryToStartCapture();
  concurrency::task<bool> TryToStartCapture(HWND hwnd);
  bool WindowMatches(HWND hwnd) const;

  winrt::fire_and_forget OnWindowClosed();

  static void WinEventProc_NewWindowHook(
    HWINEVENTHOOK,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime);
  concurrency::task<void> OnNewWindow(HWND hwnd);

  winrt::apartment_context mUIThread;
  DXResources mDXR;
  KneeboardState* mKneeboard {nullptr};
  MatchSpecification mSpec;
  HWND mHwnd {};

  unique_hwineventhook mEventHook;
  static std::mutex gHookMutex;
  static std::map<HWINEVENTHOOK, std::weak_ptr<WindowCaptureTab>> gHooks;
};

}// namespace OpenKneeboard
