#pragma once

#include <OpenKneeboard/Tab.h>

#include <filesystem>

namespace OpenKneeboard {

class DCSTab : public Tab {
 public:
  DCSTab(const wxString& title);
  virtual ~DCSTab();

  virtual void OnGameEvent(const GameEvent&) override final;

 protected:
  virtual const char* GetGameEventName() const = 0;
  virtual void Update(
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    const std::string& value)
    = 0;

  virtual void OnSimulationStart();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void Update();
};

}// namespace OpenKneeboard
