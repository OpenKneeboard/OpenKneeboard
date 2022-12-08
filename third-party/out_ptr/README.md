# ztd.out_ptr

`ztd.out_ptr` is a simple parameter wrapper for output pointers.

[![Linux & Max OSX Build Status](https://travis-ci.org/ThePhD/out_ptr.svg?branch=master)](https://travis-ci.org/ThePhD/out_ptr)
[![Build status](https://ci.appveyor.com/api/projects/status/aj895ac668xa8jo0?svg=true)](https://ci.appveyor.com/project/ThePhD/out-ptr)


# Quick Comparison and Example

```cpp
// Before
std::unique_ptr<Obj, ObjDeleter> ptr;

if (T* tmp_ptr; !c_api_get_obj(&tmp_ptr, ...)) {
  throw std::runtime_error(...);
}
else {
  ptr.reset(tmp_ptr);
}

use(ptr);
```

```cpp
// After
namespace zop = ztd::out_ptr;
std::unique_ptr<Obj, ObjDeleter> ptr;

if (!c_api_get_obj(zop::inout_ptr(ptr), ...)) {
  throw std::runtime_error(...);
}

use(ptr);
```


# Full Examples and Documentation

There are examples and documentation contained in this repository: please, peruse them as much as you need to! Some interesting/illuminating ones:

- It works with [custom unique pointers just fine](examples/source/std.custom_unique_ptr.cpp)
- It is [customizable to your own pointer types](examples/source/custom.handle.cpp), if you need performance or different semantics
- It works with [Boost](examples/source/boost.shared_ptr.cpp) and [Standard](examples/source/std.shared_ptr.cpp) shared pointers.
- It works with things like [unique_resource](https://github.com/okdshin/unique_resource) out of the box.


# Running Tests

Right now, can be run easily VIA CMake. To ease development, all necessary dependencies -- including other Boost dependencies -- are included as submodules. You can initialize and update all submodules by performing a successful `git submodule update --init --recursive` call.

From there, CMake is run. It requires that the parameters `ZTD_OUT_PTR_TESTS` is `ON`. Examples can also be run by specifying `ZTD_OUT_PTR_EXAMPLES` and `ZTD_OUT_PTR_TESTS` to be `ON` at the same time:

```bash
md out_ptr-build
cd out_ptr-build
cmake path/to/out_ptr/src -GNinja -DZTD_OUT_PTR_TESTS=ON -DZTD_OUT_PTR_EXAMPLES=ON
cmake --build .
ctest --output-on-failure
```

You can replace the `-G` argument with the generator of your choice. You may also add the `-DCMAKE_BUILD_TYPE=Debug|Release` to test certain build types. If you do, make sure to specify it on the build and test lines as well with `cmake --build . --config Debug|Release` and `ctest --output-on-failure --build-config=Debug|Release`.


# Running Benchmarks

Benchmarks can be run by running CMake with the option `-DZTD_OUT_PTR_BENCHMARKS` set to `ON`.
