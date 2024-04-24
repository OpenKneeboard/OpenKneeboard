# Contributing

For code contributions, please look over [docs/internals/](docs/internals/)

All new contributors will be required to agree to a non-exclusive Contributor License Agreement via https://cla-assistant.io/OpenKneeboard/OpenKneeboard as the current license may be too restrictive for future development. The Contributor License Agreement gives me the ability to relicense OpenKneeboard in the future, for example:

* to use libraries that is not compatible with the current license, e.g. WinUI3 (as of Feburary 2022) or GhostScript (potentially improving PDF support)
* to use artwork that is not compatible with the current license, e.g. GlyphIcons

**IMPORTANT**: To avoid potential future issues, the CLA is not limited to circumstances like the above; it would, for example, also allow me to use any contributions in proprietary software.

## clang-tidy

All C++ source and header files must pass the version of `clang-tidy` included with Visual Studio 2022.

To run clang-tidy against the entire project (this must pass):

```
msbuild /t:ClangTidy com.openkneeboard.sln`
```

To run against a specific file, from a Visual Studio command prompt, first, build the project normally; then, from a Visual Studio command prompt:

```
clang-tidy -p build src/foo/bar.cpp
```

This assumes that you build inside the `build` subdirectory, which is the default for Visual Studio Code.

After you have build the project, you can also use "C/C++: Run code analysis" from within Visual Studio Code; you may need to restart Visual Studio Code after the first file.

### Coroutine Safety

This is the primary reason why clang-tidy may be clean: both of these are buggy:

```C++
auto example_a = [](const std::string& foo) -> winrt::fire_and_forget {
  co_await wait_for_something();
  do_stuff(foo); // `foo` may now be invalid
};
auto example_b = [&foo]() -> winrt::fire_and_forget {
  co_await wait_for_something();
  do_stuff(foo); // `foo` may now be invalid
}
```

Clang-tidy will raise errors for these; these can be fixed by:

```C++
// Take coroutine parameters by value instead of by reference
auto example_a = [](std::string foo) -> winrt_fire_and_forget {
    co_await wait_for_something();
    do_stuff(foo);
};
// Don't use captures for coroutines
auto example_b = std::bind_front(
    [](auto foo) {
        co_await wait_for_something();
        do_stuff(foo);
    },
    foo);
```

### False positives

As of 2024-04-24, the version of clang-tidy included with Visual Studio 2022 raises incorrect errors for:

- functions that take a coroutine as a parameter, and take a byref argument, even if the coroutine does not take a byref argument
- non-reference captures
- non-coroutine lambdas inside a coroutine that take a capture by reference

Despite these false positives, these checks remain enabled because they frequently detect crashes or memory corruption.

These are fixed in newer versions of clang-tidy; for now, use `// NOLINT` or `// NOLINTNEXTLINE` comments to suppress the incorrect errors, e.g.:

```C++
// not a coroutine
// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters
auto example_a = [&foo]() -> void {
    return;
};
```