# ########################################################
# ##### REMEMBER TO UPDATE CPPWINRT and WIL VERSIONS #####
# ########################################################

scoped_include(cppwinrt.cmake)
scoped_include(wil.cmake)

get_target_property(CPPWINRT_VERSION ThirdParty::CppWinRT VERSION)
get_target_property(WINDOWS_IMPLEMENTATION_LIBRARY_VERSION ThirdParty::WIL VERSION)

# This is 'v1.5.4'
set(WINDOWS_APP_SDK_VERSION "1.5.240607001" CACHE INTERNAL "")
set(WINDOWS_SDK_BUILDTOOLS_VERSION "10.0.22621.756" CACHE INTERNAL "")

function(target_link_nuget_packages TARGET)
  set_property(
    TARGET "${TARGET}"
    APPEND
    PROPERTY VS_PACKAGE_REFERENCES
    ${ARGN}
  )
  get_target_property(VS_PACKAGE_REFERENCES "${TARGET}" VS_PACKAGE_REFERENCES)
  list(REMOVE_DUPLICATES VS_PACKAGE_REFERENCES)
  set_target_properties(
    "${TARGET}"
    PROPERTIES
    VS_PACKAGE_REFERENCES "${VS_PACKAGE_REFERENCES}"
    VS_GLOBAL_NuGetTargetMoniker "native,Version=v0.0"
  )
endfunction()

function(target_link_windows_app_sdk TARGET)
  target_link_nuget_packages(
    "${TARGET}"
    "Microsoft.Windows.CppWinRT_${CPPWINRT_VERSION}"
    "Microsoft.WindowsAppSDK_${WINDOWS_APP_SDK_VERSION}"
    "Microsoft.Windows.SDK.BuildTools_${WINDOWS_SDK_BUILDTOOLS_VERSION}"
    "Microsoft.Windows.ImplementationLibrary_${WINDOWS_IMPLEMENTATION_LIBRARY_VERSION}"
  )
endfunction()