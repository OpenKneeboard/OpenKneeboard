#pragma once

#include "OpenKneeboard/FolderTab.h"

namespace OpenKneeboard {

class DCSTerrainTab final : public FolderTab {
  public:
    DCSTerrainTab();

    virtual void OnGameEvent(const GameEvent&) override;
  private:
    void LoadTerrain(const std::string& terrain);

    std::string mTerrain;
};

}
