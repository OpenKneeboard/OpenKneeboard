# Build from source because DCS needs `lua.dll`, not `lua51.dll`
include(FetchContent)

FetchContent_Declare(
  pdfium_fetch
  URL "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F5296/pdfium-win-x64.tgz"
  URL_HASH "SHA256=d0a96c068b759a4040aed50bc8af97be14e52d4f11d347fef1f87ad53e7a0349"
)

FetchContent_GetProperties(pdfium_fetch)
FetchContent_MakeAvailable(pdfium_fetch)

set(PDFium_DIR "${pdfium_fetch_SOURCE_DIR}")
find_package(PDFium REQUIRED)

set_target_properties(
  pdfium
  PROPERTIES
  IMPORTED_GLOBAL true
)

add_library(ThirdParty::PDFium ALIAS pdfium)

install(
	FILES
	"${pdfium_fetch_SOURCE_DIR}/LICENSE"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-PDFium.txt"
)
