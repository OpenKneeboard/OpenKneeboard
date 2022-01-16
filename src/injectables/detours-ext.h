#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

#include <memory>

class DetourTransaction final {
 public:
  DetourTransaction();
  ~DetourTransaction();
};
