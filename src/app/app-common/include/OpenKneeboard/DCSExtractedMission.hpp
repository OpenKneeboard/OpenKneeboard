// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <filesystem>
#include <memory>

namespace OpenKneeboard {

class DCSExtractedMission final {
 public:
  DCSExtractedMission(const DCSExtractedMission&) = delete;
  DCSExtractedMission& operator=(const DCSExtractedMission&) = delete;

  DCSExtractedMission();
  ~DCSExtractedMission() noexcept;

  static std::shared_ptr<DCSExtractedMission> Get(
    const std::filesystem::path& zipPath);

  std::filesystem::path GetZipPath() const;
  std::filesystem::path GetExtractedPath() const;

 protected:
  DCSExtractedMission(const std::filesystem::path& zipPath);

 private:
  std::filesystem::path mZipPath;
  std::filesystem::path mTempDir;
};

}// namespace OpenKneeboard
