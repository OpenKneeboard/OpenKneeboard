file(GLOB_RECURSE APP_COMMON_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_library(OpenKneeboard-App-Common STATIC ${APP_COMMON_SOURCES})
target_compile_definitions(
	OpenKneeboard-App-Common
	PUBLIC
	"DIRECTINPUT_VERSION=0x0800"
	"OPENKNEEBOARD_JSON_SERIALIZE=1"
)
target_link_libraries(
	OpenKneeboard-App-Common
	PUBLIC
	D2d1
	OpenKneeboard-D2DErrorRenderer
	OpenKneeboard-DXResources
	OpenKneeboard-GameEvent
	OpenKneeboard-GetSystemColor
	OpenKneeboard-RayIntersectsRect
	OpenKneeboard-RuntimeFiles
	OpenKneeboard-OpenVRKneeboard
	OpenKneeboard-scope_guard
	OpenKneeboard-SHM
	OpenKneeboard-OpenXRMode
	OpenKneeboard-UTF8
	OpenKneeboard-config
	OpenKneeboard-dprint
	OpenKneeboard-games
	OpenKneeboard-tablets
	OpenKneeboard-version
	ThirdParty::DirectXTK
	ThirdParty::JSON
	ThirdParty::OpenVR
	ThirdParty::QPDF
	ThirdParty::LibZip
)
target_include_directories(
	OpenKneeboard-App-Common
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/UserInput/include
	${CMAKE_CURRENT_SOURCE_DIR}/GamesList/include
	${CMAKE_CURRENT_SOURCE_DIR}/TabsList/include
	${CMAKE_CURRENT_SOURCE_DIR}/tabs/include
)

add_library(OpenKneeboard::App::Common ALIAS OpenKneeboard-App-Common)
