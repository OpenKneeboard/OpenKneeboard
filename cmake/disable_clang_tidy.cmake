include_guard(GLOBAL)

# For targets with no C/C++
macro(disable_clang_tidy TARGET)
  set_target_properties(
    "${TARGET}"
    PROPERTIES
    VS_GLOBAL_ClangTidyToolPath "${CMAKE_SOURCE_DIR}/third-party"
    VS_GLOBAL_ClangTidyToolExe clang-tidy-stub.bat
  )
endmacro()