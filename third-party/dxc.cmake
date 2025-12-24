include_guard(GLOBAL)

# DXC is included in the windows SDK, but that version has SPIR-V (Vulkan support) disabled
find_package(directx-dxc CONFIG REQUIRED)
add_executable(ThirdParty::dxc IMPORTED GLOBAL)
set_target_properties(
    ThirdParty::dxc
    PROPERTIES
    IMPORTED_LOCATION "${DIRECTX_DXC_TOOL}"
)