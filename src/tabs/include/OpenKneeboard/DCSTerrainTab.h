#pragma once

#include "OpenKneeboard/DCSTab.h"

namespace OpenKneeboard {

class DCSTerrainTab final : public DCSTab {
 public:
  DCSTerrainTab();

 protected:
  virtual const char* GetGameEventName() const override final;
  virtual void Update(const std::filesystem::path&, const std::string&);
};

}// namespace OpenKneeboard
