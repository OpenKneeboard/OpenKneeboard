import ctypes
import pathlib
import os
import sys
import winreg


def openkneeboard_dll_path():
    try:
        override = os.environ["OPENKNEEBOARD_CAPI_DLL"]
        if override:
            return override
    except Exception:
        pass

    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            "Software\\Fred Emmott\\OpenKneeboard",
            0,
            winreg.KEY_READ | winreg.KEY_WOW64_64KEY,
        )
        value, result = winreg.QueryValueEx(key, "InstallationBinPath")
        if result == winreg.REG_SZ and type(value) is str:
            return value + "\\OpenKneeboard_CAPI64.dll"
    except Exception:
        pass

    # Remove this once v1.8.4+ are widespread
    program_files = os.environ["ProgramFiles"]
    if not program_files:
        sys.exit("Could not locate program files")
    path = (
        pathlib.Path(program_files)
        / "OpenKneeboard"
        / "bin"
        / "OpenKneeboard_CAPI64.dll"
    )
    if not os.path.exists(path):
        sys.exit(
            f"'{path}' does not exist; install OpenKneeboard, or set the OPENKNEEBOARD_CAPI_DLL environment variable"
        )
    return path


def openkneeboard_send(name, value):
    ok_capi = ctypes.CDLL(openkneeboard_dll_path())

    # Optional, but catches errors sooner:
    ok_capi.OpenKneeboard_send_wchar_ptr.argtypes = [
        ctypes.c_wchar_p,
        ctypes.c_size_t,
        ctypes.c_wchar_p,
        ctypes.c_size_t,
    ]

    ok_capi.OpenKneeboard_send_wchar_ptr(name, len(name), value, len(value))


if __name__ == "__main__":
    argc = len(sys.argv)
    if argc < 2:
        name = "RemoteUserAction"
        value = "NEXT_TAB"
        print(
            "Usage: python capi-test.py NAME [VALUE]\n\nSwitching to NEXT_TAB as an example."
        )
    elif argc == 2:
        name = sys.argv[1]
        value = ""
    elif argc == 3:
        name = sys.argv[1]
        value = sys.argv[2]
    elif argc > 3:
        print("Usage: python capi-test.py NAME [VALUE]\n")
        sys.exit()
    openkneeboard_send(name, value)
    print("Sent to OpenKneeboard!")
