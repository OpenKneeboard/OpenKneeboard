install(
	FILES
	enable-debugview.reg
	enable-full-dumps.reg
	enable-mini-dumps.reg
	enable-log-files.reg
	OpenKneeboard.wprp
	OpenKneeboard-with-d3d11.wprp
	DESTINATION scripts/
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.targets"
  "${CMAKE_BINARY_DIR}/Directory.Build.targets"
  COPYONLY
)

cmake_path(NATIVE_PATH CMAKE_CXX_COMPILER NATIVE_PATH_CMAKE_CXX_COMPILER)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/make-compile-commands.in.ps1"
	"${CMAKE_BINARY_DIR}/make-compile-commands.ps1"
	@ONLY
)
