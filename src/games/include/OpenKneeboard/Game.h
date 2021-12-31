#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <vector>
#include <filesystem>

namespace OpenKneeboard {

  class Game {
    public:
      Game();
      virtual ~Game();

      virtual wxString GetUserFriendlyName(const std::filesystem::path&) const = 0;
      virtual std::vector<std::filesystem::path> GetInstalledPaths() const = 0;
      virtual bool DiscardOculusDepthInformationDefault() const;
  };
}
