---
parent: Internals
---

# Detours and Hooks

Most `Hook` classes have the following structure:

```C++
class FooHook final {
  public:
   FooHook();
   ~FooHook();

   struct Callbacks {
     /* ... */
     std::function<void(const decltype(&SomeFunc)& next)> onFoo;
   };

   InstallHook(const Callbacks&);
   UninstallHook();
};
```

This feels a bit strange, non-RAII, but it is required because of concurrency in the programs we're injected in to.

## A separate initialization function (`InstallHook()`) is neccessary

If the hook is initialized in the constructor, it is possible for another thread to invoke the hook *immediately* - before the rest of the constructor has initialized.

For example:

```c++
MyClass::MyClass() {
  mFoo = std::make_unique<FooHook>(Callbacks {
    .onFoo = std::bind_front(&MyClass::OnFoo, this);
  });
}

void MyClass::OnFoo() {
  // May crash: the hook may have been called by another thread
  // before std::make_unique returned in the constructor
  mFoo->Bar();
}
```

This specific problem could be avoided by making `FooHack` abstract instead of final, and replacing the callbacks with pure virtual functions. However, this would still require a separate initialization function:

If the superclass constructor initializes the hook, it is possible for the hook to be invoked before all the subclass constructors have been invoked - and in turn, before the vtables have been initialzied, so it results in pure virtual function calls.

These are not just theoretical concerns; they are reliably reproducible with the Direct3D hook in particular, especially when injecting early in the process lifetime.

## A separate teardown function (`UninstallHook`) and destructor is necessary

In most cases, the hook callback depends is not a pure function - i.e. it depends on other properties or functions in the class. This means that when the hook is invoked, the object must still be valid.

If the hook is only uninstalled when the hook class is automatically destructed, if calls are in progress, they may still attempt to use parts of the object that have already freed.

It is useful to call `UninstallHook()` from your destructor; the shared `DllMain()` will call it on the top level object, wait 100ms for any in-flight calls to finish, then start freeing memory.

## Function pointer references (`const decltype(&SomeFunc)& next`)

Taking pointers by reference is strange; this is done here as it can change: usually, when the hook is invokved, `next` will contain the reference of the trampoline - however, if `UninstallHook()` is called while another thread's call to the hook is in progress, the trampoline will become invalid - but `next` will be changed back to point to the real function.

Using a reference keeps in-flight calls working when `UninstallHook()` is called.
