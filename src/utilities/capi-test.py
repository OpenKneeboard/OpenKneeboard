import ctypes
import pathlib
import os
import sys


def openkneeboard_dll_path():
    override = os.environ["OPENKNEEBOARD_CAPI_DLL"]
    if override:
        return override
    program_files = os.environ["ProgramFiles"]
    if not program_files:
        sys.exit("Could not locate program files")
    path = pathlib.Path(program_files) / "OpenKneeboard" / \
        "bin" / "OpenKneeboard_CAPI.dll"
    if not os.path.exists(path):
        sys.exit(
            f"'{path}' does not exist; install OpenKneeboard, or set the OPENKNEEBOARD_CAPI_DLL environment variable")
    return path


def openkneeboard_send(name, value):
    ok_capi = ctypes.CDLL(openkneeboard_dll_path())

    # Optional, but catches errors sooner:
    ok_capi.OpenKneeboard_send_wchar_ptr.argtypes = [
        ctypes.c_wchar_p, ctypes.c_size_t, ctypes.c_wchar_p, ctypes.c_size_t]

    ok_capi.OpenKneeboard_send_wchar_ptr(
        name, len(name), value, len(value))


if __name__ == "__main__":
    argc = len(sys.argv)
    if argc < 2:
        name = "RemoteUserAction"
        value = "NEXT_TAB"
        print(
            "Usage: python capi-test.py NAME [VALUE]\n\nSwitching to NEXT_TAB as an example.")
    elif argc == 2:
        name = sys.argv[1]
        value = ''
    elif argc == 3:
        name = sys.argv[1]
        value = sys.argv[2]
    elif argc > 3:
        print("Usage: python capi-test.py NAME [VALUE]\n")
        sys.exit()
    openkneeboard_send(name, value)
    print("Sent to OpenKneeboard!")
