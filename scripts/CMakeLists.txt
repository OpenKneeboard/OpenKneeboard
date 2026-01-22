set(
  SCRIPT_FILES
  enable-debugview.reg
  enable-full-dumps.reg
  enable-mini-dumps.reg
  enable-log-files.reg
  OpenKneeboard.wprp
  OpenKneeboard-with-d3d11.wprp
)

foreach (FILE IN LISTS SCRIPT_FILES)
  file(
    GENERATE
    OUTPUT "${BUILD_OUT_ROOT}/scripts/${FILE}"
    INPUT "${FILE}"
  )
endforeach ()

add_executable(args-to-lines args-to-lines.cpp)
add_executable(true "true.cpp")
