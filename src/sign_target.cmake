if(WIN32)
  # Use the nuget version to be certain it's new enough to support
  # https://docs.microsoft.com/en-us/windows/msix/package/persistent-identity
  set(
    SIGNTOOL_EXE
    "${NUGET_WINDOWS_SDK_BUILD_TOOLS_PATH}/bin/${NUGET_WINDOWS_SDK_BUILD_TOOLS_COMPATIBILITY_VERSION}/x64/signtool.exe"
  )
endif()
function(sign_target TARGET)
  if(SIGNTOOL_KEY_ARGS AND WIN32)
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      COMMAND
      "${SIGNTOOL_EXE}"
      ARGS
      sign
      ${SIGNTOOL_KEY_ARGS}
      /t http://timestamp.digicert.com
      /fd SHA256
      "$<TARGET_FILE:${TARGET}>"
      DEPENDS
      WindowsSDKBuildToolsNuget
    )
  endif()
endfunction()
