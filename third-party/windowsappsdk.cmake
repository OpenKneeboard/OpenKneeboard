# ###################################################################
# ##### REMEMBER TO UPDATE CPPWINRT_VERSION IN `cppwinrt.cmake` #####
# ###################################################################

# This is 'v1.5.1'
set(WINDOWS_APP_SDK_VERSION "1.5.240211001" CACHE INTERNAL "")
set(WINDOWS_SDK_BUILDTOOLS_VERSION "10.0.22621.756" CACHE INTERNAL "")
set(WINDOWS_IMPLEMENTATION_LIBRARY_VERSION "1.0.240122.1" CACHE INTERNAL "")

function(target_link_nuget_packages TARGET)
  set_property(
    TARGET "${TARGET}"
    APPEND
    PROPERTY VS_PACKAGE_REFERENCES
    ${ARGN}
  )
  get_target_property(VS_PACKAGE_REFERENCES "${TARGET}" VS_PACKAGE_REFERENCES)
  list(REMOVE_DUPLICATES VS_PACKAGE_REFERENCES)
  set_property(TARGET "${TARGET}" PROPERTY VS_PACKAGE_REFERENCES "${VS_PACKAGE_REFERENCES}")
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
