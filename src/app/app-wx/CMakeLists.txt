file(GLOB_RECURSE APP_WX_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(OpenKneeboard WIN32 ${APP_WX_SOURCES} OpenKneeboard.rc)
sign_target(OpenKneeboard)
target_link_libraries(
	OpenKneeboard
	PRIVATE
	OpenKneeboard-App-Common
	wx::base
	wx::core
)

install(TARGETS OpenKneeboard)
install(FILES "$<TARGET_PDB_FILE:OpenKneeboard>" TYPE BIN COMPONENT DebugSymbols)
