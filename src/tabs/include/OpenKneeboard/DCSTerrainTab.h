#pragma once

#include "OpenKneeboard/FolderTab.h"

namespace OpenKneeboard {

class DCSTerrainTab final : public FolderTab {
  public:
    DCSTerrainTab();

    virtual void OnGameEvent(const GameEvent&) override;
  private:
    class Impl;
    std::shared_ptr<Impl> p;

    void Update();
};

}
