# About `std::enable_shared_from_this`

This class adds `this->shared_from_this()` and `this->weak_from_this()`.

## How to use it

Ideally, don't :) Check 'When to use it' below.

### Make all constructors private

Add a public static factory method that returns an `std::shared_ptr<T>` instead. In OpenKneeboard, these static methods are usually called `T::Create()`.

It is an error to call `this->shared_from_this()` or `this->weak_from_this()`, unless the object lifetime is managed by an `std::shared_ptr<T>`. Making the constructors private guarantees that callers *must* use the factory method, making it safe to use the new methods.

### Do not call use from constructors, or methods called by constructors

The `shared_ptr<T>` is initialized after `T`, so it is invalid to call `this->shared_from_this()` or `this->weak_from_this()` from `T`'s constructor.

You may need to move some initialization to your factory method, or a (private) post-construction init function.

## When to use it

In traditional procedural code, it is usually an antipattern: code shouldn't care how its' memory is managed.

Two common techniques in OpenKneeboard (and C++/WinRT) change this:

- async callbacks
- coroutines

These are largely equivalent problems: code will be called *later*, by code we're not in control of, and potentially in another thread.

For example, these examples are unsafe, as `this` may be deleted before - or while - the method is being invoked:

```C++
// Async
foo.OnSomeEvent(std::bind_front(&MyClass::SomeMethod, this));
```

```C++
// Coroutines
winrt::fire_and_forget MyClass::MyCoroutineMethod() {
  // Safe, as long as:
  // - liveness was guaranteed when `MyCoroutineMethod` was called
  // - the coroutine type's `initial_suspend()` returns an `std::suspend_never`
  this->SomeMethod();
  co_await doSomeStuff();
  // NOT SAFE
  this->SomeMethod();
}
```

We need to:

- detect if `this` has been deleted before the callback: we need at least a `weak_ptr`
- avoid a refcount loop: we don't want to capture a `shared_ptr`
- make sure that `this` stays alive for the duration of the callback: we need to `lock()` the `weak_ptr` to get a `shared_ptr`

So:

```C++
// Callback
foo.OnSomeEvent(
  [weakThis = this->weak_from_this()]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }
    strongThis->SomeMethod();
  })
);
```

```C++
winrt::fire_and_forget MyClass::MyCoroutineMethod() {
  // Safe, as long as:
  // - liveness was guaranteed when `MyCoroutineMethod` was called
  // - the coroutine type's `initial_suspend()` returns an `std::suspend_never`
  const auto self = shared_from_this();

  self->SomeMethod();
  co_await doSomeStuff();
  // Now safe
  self->SomeMethod();
}
```

The lambda callback case is pretty verbose, so OpenKneeboard includes `OpenKneeboard::weak_wrap()` to make things a bit better:

```C++
foo.OnSomeEvent(
  [](auto self) {
    // `self` is an `std::shared_ptr<decltype(this)>`
    self->SomeMethod();
  },
  this);
```

## Tangent: captures in coroutine lambdas

```C++
auto lambda = [this, bar] () -> winrt::fire_and_forget {
  // Only safe *IF* the coroutine's `initial_suspend()` returns
  // an `std::suspend_never`:
  this->DoStuff();
  bar.DoStuff();
  co_await baz();
  // **NEVER** safe:
  this->DoStuff();
  bar.DoStuff();
};
```

Lambda captures are only guaranteed to be valid for the lifetime of the lambda - which *returns* a coroutine. Once the coroutine is started:
- the lambda may no longer exist
- even if it does, the captures may be referencing stack values which are no longer valid
- take parameters by value

**DO NOT USE CAPTURES FOR COROUTINE LAMBDAS**. Instead:

```C++
auto lambda = [this, bar] () {
  return [](auto& weakThis, auto bar) -> winrt::fire_and_forget {
    // ...
  }(weak_from_this(), bar);
}();
```

## Further reading

- https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/weak-references
- https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/handle-events#safely-accessing-the-this-pointer-with-an-event-handling-delegate
- https://kennykerr.ca/2018/07/16/new-in-cppwinrt-mastering-strong-and-weak-references/
- https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rcoro-capture
