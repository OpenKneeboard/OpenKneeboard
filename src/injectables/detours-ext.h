#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

void DetourUpdateAllThreads();
