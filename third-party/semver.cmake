find_path(
  SEMVER_INCLUDE_PATH
  semver.hpp
  REQUIRED
)
add_library(ThirdParty::SemVer INTERFACE IMPORTED GLOBAL)
target_include_directories(
  ThirdParty::SemVer
  INTERFACE "${SEMVER_INCLUDE_PATH}"
)