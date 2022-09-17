add_library(
  vrperfkit
  STATIC
  EXCLUDE_FROM_ALL
  vrperfkit/d3d11_helper.cpp
)
target_include_directories(
  vrperfkit
  PUBLIC
  vrperfkit/include
)

add_library(ThirdParty::vrperfkit ALIAS vrperfkit)
install(
	FILES "vrperfkit/LICENSE.txt"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-vrperfkit.txt"
)
