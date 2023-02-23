---
parent: Internals
---

# Coding Style

* use standard C++ features where possible
* use `c++/winrt` for COM
* use `winrt::check_hresult {}` for any hresult API calls that are required
* prefer smart pointers over raw pointers

## Whitespace

Use `clang-format`. If you use Visual Studio Code, the built-in
'format document'/'format range' features will do this.

## Naming Conventions

As the libraries used by OpenKneeboard follow different conventions, it's not
possible to be consistent with all of them. The goal is to be as consistent as
possible while avoid conflicts; for example, properties and class names should not have the same convention.

* use US English; for example, call a `D2D1_COLOR_F` `myColor`, not `myColour`
* most classes should be `UpperCamelCase`
* functions and methods should be `UpperCamelCase`
* 'stdlib-like' definitions should be `lower_underscored` (e.g. `dprint()`,
  `scope_guard`)
* local variables should be `lowerCamelCase`
* private/protected properties should be `mUpperCamelCase`
* static variables should be `sUpperCamelCase`
* global variables should be `gUpperCamelCase`
