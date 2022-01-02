#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <filesystem>

namespace OpenKneeboard {

wxIcon GetIconFromExecutable(const std::filesystem::path& path);

}
