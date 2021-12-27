#pragma once

#include "OpenKneeboard/FolderTab.h"

#include <filesystem>

namespace OpenKneeboard {

class DCSTab : public FolderTab {
  public:
    DCSTab(const wxString& title);

    virtual void OnGameEvent(const GameEvent&) override final;
  protected:
    virtual const char* GetGameEventName() const = 0;
    virtual void Update(const std::filesystem::path& , const std::string&) = 0;
  private:
    class Impl;
    std::shared_ptr<Impl> p;

    void Update();
};

}
