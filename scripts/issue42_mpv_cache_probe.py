"""Pinned libmpv native property contract, without playing any media.

Run on Windows BRG after bootstrap-deps.ps1 has installed the SHA-pinned DLL.
This probes the live libmpv property registry, not only documentation or
source-string spelling. No URL, network request, MPV loadfile, audio output,
secrets or shell subprocesses are used.
"""
import ctypes
import os
import pathlib
import sys

MPV_FORMAT_FLAG = 3
MPV_ERROR_PROPERTY_NOT_FOUND = -8
MPV_ERROR_PROPERTY_UNAVAILABLE = -10


def main():
    if sys.platform != "win32":
        raise SystemExit("Pinned libmpv probe must run on Windows hosted runner")
    root = pathlib.Path(__file__).resolve().parents[1]
    dll_path = root / "deps" / "mpv" / "libmpv-2.dll"
    if not dll_path.is_file():
        raise SystemExit("Pinned libmpv-2.dll missing (bootstrap not run)")
    with os.add_dll_directory(str(dll_path.parent)):
        lib = ctypes.WinDLL(str(dll_path))
        lib.mpv_create.restype = ctypes.c_void_p
        lib.mpv_initialize.argtypes = [ctypes.c_void_p]
        lib.mpv_initialize.restype = ctypes.c_int
        lib.mpv_get_property.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p
        ]
        lib.mpv_get_property.restype = ctypes.c_int
        lib.mpv_terminate_destroy.argtypes = [ctypes.c_void_p]
        lib.mpv_terminate_destroy.restype = None

        handle = lib.mpv_create()
        if not handle:
            raise SystemExit("mpv_create failed before property probe")
        try:
            init_rc = lib.mpv_initialize(handle)
            if init_rc < 0:
                raise SystemExit(f"mpv_initialize returned {init_rc}")

            def query(name):
                value = ctypes.c_int()
                return lib.mpv_get_property(
                    handle, name.encode("ascii"), MPV_FORMAT_FLAG,
                    ctypes.byref(value)
                )

            correct = query("paused-for-cache")
            wrong = query("pause-for-cache")
            print(f"MPV_CACHE_PROPERTY_PROBE canonical_rc={correct} legacy_rc={wrong} "
                  "media_playback=NOT_RUN", flush=True)
            # The property can be unavailable before a file is loaded,
            # but it must not be unknown to this exact pinned libmpv binary.
            if correct not in (0, MPV_ERROR_PROPERTY_UNAVAILABLE):
                raise SystemExit(
                    f"Canonical paused-for-cache unrecognized: rc={correct}"
                )
            # A separate cache-pause OPTION does not authorize using
            # pause-for-cache as an observed runtime PROPERTY.
            if wrong == 0:
                raise SystemExit("Noncanonical pause-for-cache unexpectedly queried")
            print("MPV_CACHE_PROPERTY_PROBE PASS (runtime registry only)", flush=True)
        finally:
            lib.mpv_terminate_destroy(handle)


if __name__ == "__main__":
    main()
