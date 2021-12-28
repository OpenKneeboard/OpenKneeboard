#include "detours-ext.h"

#include <TlHelp32.h>

#include <OpenKneeboard/dprint.h>

using OpenKneeboard::dprintf;

void DetourUpdateAllThreads() {
	auto snapshot
		= CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
	THREADENTRY32 thread;
	thread.dwSize = sizeof(thread);

	DetourUpdateThread(GetCurrentThread());
	auto myProc = GetCurrentProcessId();
	auto myThread = GetCurrentThreadId();
	if (!Thread32First(snapshot, &thread)) {
		dprintf("Failed to find first thread");
		return;
	}

	do {
		if (!thread.th32ThreadID) {
			continue;
		}
		if (thread.th32OwnerProcessID != myProc) {
			// CreateToolhelp32Snapshot takes a process ID, but ignores it
			continue;
		}
		if (thread.th32ThreadID == myThread) {
			continue;
		}

		auto th = OpenThread(THREAD_ALL_ACCESS, false, thread.th32ThreadID);
		DetourUpdateThread(th);
	} while (Thread32Next(snapshot, &thread));
}
