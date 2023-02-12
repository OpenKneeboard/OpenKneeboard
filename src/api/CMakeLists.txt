add_module_library(
	OpenKneeboard-lua-api
	OpenKneeboard_LuaAPI.cpp)
target_link_libraries(OpenKneeboard-lua-api PUBLIC lualib)
target_link_libraries(
	OpenKneeboard-lua-api
	PRIVATE
	lualib
	OpenKneeboard-dprint
	OpenKneeboard-GameEvent)
set_target_properties(
	OpenKneeboard-lua-api
	PROPERTIES
	OUTPUT_NAME "OpenKneeboard_LuaAPI")

add_module_library(
	OpenKneeboard-c-api
	OpenKneeboard_CAPI.cpp)
target_link_libraries(
	OpenKneeboard-c-api
	PRIVATE
	OpenKneeboard-dprint
	OpenKneeboard-GameEvent)
set_target_properties(
	OpenKneeboard-c-api
	PROPERTIES
	OUTPUT_NAME "OpenKneeboard_CAPI")
target_include_directories(
	OpenKneeboard-c-api
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
install(
	FILES
	"OpenKneeboard_CAPI.h"
	DESTINATION "include")
