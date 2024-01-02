add_subdirectory(openxr-toolkit EXCLUDE_FROM_ALL)

install(
	FILES "openxr-toolkit/LICENSE"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-OpenXR-Toolkit.txt"
)