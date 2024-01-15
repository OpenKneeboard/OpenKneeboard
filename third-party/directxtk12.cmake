include(ExternalProject)
ExternalProject_Add(
  directxtk12Build
  URL "https://github.com/microsoft/DirectXTK12/archive/refs/tags/dec2023.zip"
  URL_HASH "SHA256=5db73d32f82c1ed4b9b12a0868b4ae80481dbe9728aabda99a5617be4fcc4624"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK12.lib"
  CMAKE_ARGS
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  -DBUILD_TOOLS=OFF
  -DBUILD_XAUDIO_WIN10=OFF
  -DBUILD_XAUDIO_WIN8=OFF
  -DBUILD_XAUDIO_WIN7=OFF
  -DDIRECTX_ARCH="$<IF:$<EQUAL:${BUILD_BITNESS},64>,x64,x86>"

  # Split the install dir by configuration so we don't have mismatches for ITERATOR_DEBUG_LEVEL
  # or MSVCRT
  INSTALL_COMMAND
  ${CMAKE_COMMAND} --install . "--prefix=<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

add_library(directxtk12 INTERFACE)
add_dependencies(directxtk12 directxtk12Build)

ExternalProject_Get_property(directxtk12Build INSTALL_DIR)
target_include_directories(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTK12.lib")

ExternalProject_Get_property(directxtk12Build SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-DirectXTK12.txt"
)

add_library(ThirdParty::DirectXTK12 ALIAS directxtk12)
