ExternalProject_Add(
  vulkanHeadersEP
  URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.241.zip"
  URL_HASH "SHA256=967ef8bf6ec22fdd9f92d0a2396e0203b86112ceea4fe675cf6f2efffdcaef53"
  BUILD_BYPRODUCTS "<SOURCE_DIR>/include"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

ExternalProject_Get_property(vulkanHeadersEP SOURCE_DIR)

add_library(vulkan-headers INTERFACE)
add_dependencies(vulkan-headers vulkanHeadersEP)
target_include_directories(vulkan-headers INTERFACE "${SOURCE_DIR}/include")
target_compile_definitions(vulkan-headers INTERFACE VK_USE_PLATFORM_WIN32_KHR)

add_library(ThirdParty::VulkanHeaders ALIAS vulkan-headers)

include(ok_add_license_file)
ok_add_license_file(
  "${SOURCE_DIR}/LICENSE.txt"
  "LICENSE-ThirdParty-Vulkan-SDK.txt"
  DEPENDS vulkanHeadersEP-update
)