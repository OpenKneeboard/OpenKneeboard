add_library(System::D2d1 INTERFACE IMPORTED GLOBAL)
set_property(TARGET System::D2d1 PROPERTY IMPORTED_LIBNAME "D2d1")

add_library(System::Dwrite INTERFACE IMPORTED GLOBAL)
set_property(TARGET System::Dwrite PROPERTY IMPORTED_LIBNAME "Dwrite")

add_library(System::WindowsApp INTERFACE IMPORTED GLOBAL)
set_property(TARGET System::WindowsApp PROPERTY IMPORTED_LIBNAME "WindowsApp")
