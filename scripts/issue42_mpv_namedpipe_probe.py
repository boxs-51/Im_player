"""Issue42 PIPE01: real Win32 named-pipe bytes from pinned libmpv ao=pcm.

Windows CI only. An isolated test-owned named-pipe server receives raw
float32/48kHz/stereo PCM BEFORE a controlled, late AUDIO-only HLS 503.
The same pinned libmpv run retains MID01/MID02 source selection and video
post-error progress oracles. This does NOT execute AudioCaptureManager,
AudioProcessor, SDL or a physical audio device. Data read after 503 may be
predecoded/buffered and cannot prove post-fault PCM decoder continuity.
"""
import ctypes
from ctypes import wintypes
import math
import os
from pathlib import Path
import struct
import sys
import threading
import uuid

from issue42_mpv_midstream_probe import experiment

ERROR_PIPE_CONNECTED = 535
ERROR_BROKEN_PIPE = 109
ERROR_NO_DATA = 232
MAX_BYTES = 32 * 1024 * 1024
MIN_FRAMES = 2400


class OwnedPipeReader:
    def __init__(self):
        if sys.platform != "win32":
            raise RuntimeError("PIPE01_WINDOWS_REQUIRED")
        k = ctypes.WinDLL("kernel32", use_last_error=True)
        k.CreateNamedPipeW.argtypes = [
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
            wintypes.DWORD, ctypes.c_void_p,
        ]
        k.CreateNamedPipeW.restype = wintypes.HANDLE
        k.ConnectNamedPipe.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
        k.ConnectNamedPipe.restype = wintypes.BOOL
        k.ReadFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                               wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                               ctypes.c_void_p]
        k.ReadFile.restype = wintypes.BOOL
        k.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
        k.DisconnectNamedPipe.restype = wintypes.BOOL
        k.CloseHandle.argtypes = [wintypes.HANDLE]
        k.CloseHandle.restype = wintypes.BOOL
        self.k = k
        self.path = (r"\\.\pipe\issue42_aopcm_" +
                     str(os.getpid()) + "_" + uuid.uuid4().hex)
        self.handle = k.CreateNamedPipeW(
            self.path, 0x00000001 | 0x00080000,
            0x00000000, 1, 65536, 65536, 0, None)
        if self.handle in (None, ctypes.c_void_p(-1).value):
            raise RuntimeError("PIPE01_CREATE_NAMED_PIPE_FAILED")
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.lock = threading.Lock()
        self.connected = False
        self.total = 0
        self.finite = 0
        self.nonzero = 0
        self.remnant = b""
        self.error = None

    def _run(self):
        try:
            connected = self.k.ConnectNamedPipe(self.handle, None)
            if not connected and ctypes.get_last_error() != ERROR_PIPE_CONNECTED:
                raise RuntimeError("PIPE01_CONNECT_FAILED")
            with self.lock:
                self.connected = True
            while True:
                buf = ctypes.create_string_buffer(65536)
                n = wintypes.DWORD()
                ok = self.k.ReadFile(self.handle, buf, len(buf),
                                     ctypes.byref(n), None)
                if not ok:
                    if ctypes.get_last_error() in (ERROR_BROKEN_PIPE,
                                                  ERROR_NO_DATA):
                        break
                    raise RuntimeError("PIPE01_READ_FAILED")
                if n.value == 0:
                    break
                samplebuf = self.remnant + buf.raw[:n.value]
                aligned = len(samplebuf) // 8 * 8
                self.remnant = samplebuf[aligned:]
                finite = nonzero = 0
                for (sample,) in struct.iter_unpack("<f", samplebuf[:aligned]):
                    if not math.isfinite(sample):
                        raise RuntimeError("PIPE01_NONFINITE_PCM")
                    finite += 1
                    if abs(sample) > 1e-6:
                        nonzero += 1
                with self.lock:
                    self.total += n.value
                    self.finite += finite
                    self.nonzero += nonzero
                    if self.total > MAX_BYTES:
                        raise RuntimeError("PIPE01_PCM_SIZE_CAP_EXCEEDED")
        except BaseException as ex:
            with self.lock:
                self.error = type(ex).__name__ + ":" + str(ex)

    def start(self):
        self.thread.start()

    def snapshot(self):
        with self.lock:
            return {
                "connected": self.connected,
                "bytes": self.total,
                "finite": self.finite,
                "nonzero": self.nonzero,
                "error": self.error,
            }

    def close(self):
        self.thread.join(timeout=3)
        self.k.DisconnectNamedPipe(self.handle)
        self.k.CloseHandle(self.handle)
        if self.thread.is_alive():
            raise AssertionError("PIPE01_READER_NOT_JOINED")


def main():
    if sys.platform != "win32":
        raise SystemExit("PIPE01_PINNED_WINDOWS_LIBMPV_REQUIRED")
    dll_path = Path(__file__).resolve().parents[1] / "deps/mpv/libmpv-2.dll"
    if not dll_path.is_file():
        raise SystemExit("PIPE01_PINNED_LIBMPV_DLL_MISSING")
    pipe = OwnedPipeReader()
    before = {}
    pipe.start()
    try:
        def before_audio_fault():
            before.update(pipe.snapshot())
            if before["error"]:
                raise AssertionError("PIPE01_READER_FAILED_PREFAULT")
            if not before["connected"]:
                raise AssertionError("PIPE01_NO_NATIVE_CONNECT")
            if before["bytes"] < MIN_FRAMES * 8:
                raise AssertionError("PIPE01_NO_NATIVE_PCM_PREFAULT")
            if before["bytes"] % 8 or before["finite"] * 4 > before["bytes"]:
                raise AssertionError("PIPE01_INVALID_PCM_PREFAULT")
            if before["nonzero"] == 0:
                raise AssertionError("PIPE01_ZERO_ONLY_PCM_PREFAULT")

        with os.add_dll_directory(str(dll_path.parent)):
            result = experiment(ctypes.WinDLL(str(dll_path)),
                                pcm_output_path=pipe.path,
                                before_audio_fault=before_audio_fault)
        last = pipe.snapshot()
        if not result or result.get("audio_503_receipt_count") != 1:
            raise AssertionError("PIPE01_MIDPLAY_503_ORACLE_MISSING")
        if last["error"]:
            raise AssertionError("PIPE01_READER_FAILED")
        if last["bytes"] < before["bytes"] or last["bytes"] % 8:
            raise AssertionError("PIPE01_NONMONOTONIC_OR_UNALIGNED_BYTES")
        if last["finite"] * 4 != last["bytes"]:
            raise AssertionError("PIPE01_INCOMPLETE_FLOAT_PCM_FRAMES")
        print("I42_REAL_WIN32_PIPE_PCM_PREFAULT_PASS "
              f"bytes_pre_fault={before['bytes']} "
              f"nonzero_pre_fault={before['nonzero']} "
              f"bytes_session={last['bytes']} "
              f"finite_samples_session={last['finite']} "
              "audio_late_503_receipts=1 video_post_fault_progress=true "
              "pipe_server=TEST_ONLY_NOT_AUDIO_CAPTURE_MANAGER "
              "pcm_post_fault_decoder_continuity=UNVERIFIED "
              "app_processor=NOT_RUN sdl=NOT_RUN audible=NOT_RUN "
              "first_broken_boundary=UNKNOWN", flush=True)
    finally:
        pipe.close()


if __name__ == "__main__":
    main()
