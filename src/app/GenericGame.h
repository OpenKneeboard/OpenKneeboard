#pragma once

#include <OpenKneeboard/Game.h>

namespace OpenKneeboard {

class GenericGame final : public Game {
  public:
    virtual const char* GetNameForConfigFile() const override;
    virtual wxString GetUserFriendlyName(const std::filesystem::path&) const override;
    virtual std::vector<std::filesystem::path> GetInstalledPaths() const override;
};

}
