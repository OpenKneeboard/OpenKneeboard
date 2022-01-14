#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

#include <memory>

class DetourTransaction final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  DetourTransaction();
  ~DetourTransaction();
};
