# Coding Style

* use standard C++ features where possible
* use `c++/winrt` for COM
* use `winrt::check_hresult {}` for any hresult API calls that are required
* minimize coupling to wxWidgets
* prefer smart pointers over raw pointers

## Whitespace

Use `clang-format`. If you use Visual Studio Code, the built-in
'format document'/'format range' features will do this.

## Naming Conventions

As the libraries used by OpenKneeboard follow different conventions, it's not
possible to be consistent with all of them. The goal is to be as consistent as
possible while avoid conflicts; for example, properties and class names should not have the same convention.

* use US English; for example, call a `D2D1_COLOR_F` `myColor`, not `myColour`
  * exception: when interacting with wxWidgets, aim for consistency and
    readability. It may be better to call a `wxColour` `myColour`.
* most classes should be `UpperCamelCase`
* classes that are heavily coupled to wxWidgets - e.g. windows or config app
  UI elements - should be named `okUpperCamelCase` for consistency, and to
  clearly identify where coupling to wxWidgets happens.
* functions and methods should be `UpperCamelCase`
* 'stdlib-like' definitions should be `lower_underscored` (e.g. `dprint()`,
  `scope_guard`)
* private/protected properties should be `mUpperCamelCase`
* local variables should be `lowerCamelCase`
