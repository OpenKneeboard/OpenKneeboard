#include <Windows.h>

/** This DLL does nothing; it exists just so the main app can already
 * tell if it's injected a specific process.
 *
 * This is useful as the bootstrapper unloads itself once it's done.
 */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return true;
}
