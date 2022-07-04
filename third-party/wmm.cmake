# NOAA World Magnetic Modellibrary

set(WMM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WMM2020_Windows/src")
add_library(
  wmm
  STATIC
  EXCLUDE_FROM_ALL
  "${WMM_ROOT}/GeomagnetismLibrary.c"
)
target_include_directories(wmm PUBLIC "${WMM_ROOT}")

add_library(ThirdParty::WMM ALIAS wmm)
