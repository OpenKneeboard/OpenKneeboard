// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/task.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace OpenKneeboard {

struct DXResources;
class KneeboardState;
class IPageSource;

class FilePageSource final {
 public:
  FilePageSource() = delete;

  static std::vector<std::string> GetSupportedExtensions(
    const audited_ptr<DXResources>&) noexcept;

  static task<std::shared_ptr<IPageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    std::filesystem::path) noexcept;
};

}// namespace OpenKneeboard
