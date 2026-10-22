"""Issue #42 T1: real SHA-pinned libmpv against owned loopback HTTP A/V.

Headless Windows CI only. Distinct VIDEO (Y4M) and AUDIO (WAV) localhost
sources are served to the actual pinned libmpv-2.dll with vo=null/ao=null.
A server receipt is never treated as decoder/PCM/SDL/audible evidence.
The controller knows which URL it submitted, but there is NO interception of
libmpv's internal HTTP transport and NO app-level source_uid/epoch contract.
No external HTTP, secrets, token-bearing URLs or recovery behavior.
"""
import contextlib
import ctypes
import io
import json
import math
import os
from pathlib import Path
import struct
import sys
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MPV_FORMAT_STRING = 1
MPV_FORMAT_FLAG = 3
MPV_FORMAT_INT64 = 4
FILE_LOADED = 8
END_FILE = 7
SHUTDOWN = 1
QUEUE_OVERFLOW = 24


def wav_bytes():
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(48000)
        frames = bytearray()
        for i in range(48000 * 8):
            v = int(1800 * math.sin(2 * math.pi * 220 * i / 48000))
            frames.extend(struct.pack("<hh", v, v))
        wav.writeframes(frames)
    return output.getvalue()


def y4m_bytes():
    # ~200KB deterministic 18s visual content; no platform encoder required.
    w, h, fps = 32, 24, 10
    frame = (b"\x40" * (w * h) + b"\x80" * (w * h // 2))
    return (f"YUV4MPEG2 W{w} H{h} F{fps}:1 Ip A1:1 C420jpeg\n".encode()
            + (b"FRAME\n" + frame) * (fps * 18))


class Source:
    """Receipt is role-owned, not an audio attribution from arbitrary mpv logs."""

    def __init__(self):
        self.payloads = {"video": y4m_bytes(), "audio": wav_bytes()}
        self.audio_mode = "ok"
        self.video_mode = "ok"
        self.records = []
        self.lock = threading.Lock()

    def set_modes(self, video="ok", audio="ok"):
        with self.lock:
            self.audio_mode, self.video_mode = audio, video

    def receipt(self, role, status, delivered, declared):
        with self.lock:
            self.records.append({"role": role, "status": status,
                                 "bytes_sent": delivered, "declared_bytes": declared,
                                 "receipt_uid": len(self.records) + 1})

    def snapshot(self):
        with self.lock:
            return list(self.records)


@contextlib.contextmanager
def localhost_fixture(source):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass  # Never log URL, headers, or mpv error text.

        def do_GET(self):
            # Strict resource routing. Query strings and arbitrary paths rejected.
            if self.path not in ("/audio", "/video"):
                self.send_error(404)
                return
            role = self.path[1:]
            with source.lock:
                mode = source.audio_mode if role == "audio" else source.video_mode
                payload = source.payloads[role]
            if mode == "503":
                self.send_response(503)
                self.send_header("Content-Length", "0")
                self.end_headers()
                source.receipt(role, 503, 0, 0)
                return
            declared = len(payload) + (8192 if mode == "truncate" else 0)
            sent = payload[:1024] if mode == "truncate" else payload
            self.send_response(200)
            self.send_header("Content-Type", "audio/wav" if role == "audio"
                             else "video/x-yuv4mpeg")
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Length", str(declared))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            try:
                self.wfile.write(sent)
                self.wfile.flush()
                source.receipt(role, 200, len(sent), declared)
            except (BrokenPipeError, ConnectionResetError, OSError):
                source.receipt(role, 200, 0, declared)
            if mode == "truncate":
                self.close_connection = True

    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    srv.daemon_threads = True
    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        yield "http://127.0.0.1:%d" % srv.server_port
    finally:
        srv.shutdown()
        srv.server_close()
        thread.join(timeout=3)
        if thread.is_alive():
            raise AssertionError("HTTP_FIXTURE_THREAD_LEAK")


class Event(ctypes.Structure):
    _fields_ = [("event_id", ctypes.c_int), ("error", ctypes.c_int),
                ("reply_userdata", ctypes.c_uint64), ("data", ctypes.c_void_p)]


class RealMpv:
    def __init__(self, dll):
        self.lib = dll
        dll.mpv_create.restype = ctypes.c_void_p
        dll.mpv_set_option_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                              ctypes.c_char_p]
        dll.mpv_set_option_string.restype = ctypes.c_int
        dll.mpv_initialize.argtypes = [ctypes.c_void_p]
        dll.mpv_initialize.restype = ctypes.c_int
        dll.mpv_command.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
        dll.mpv_command.restype = ctypes.c_int
        dll.mpv_wait_event.argtypes = [ctypes.c_void_p, ctypes.c_double]
        dll.mpv_wait_event.restype = ctypes.POINTER(Event)
        dll.mpv_get_property.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                         ctypes.c_int, ctypes.c_void_p]
        dll.mpv_get_property.restype = ctypes.c_int
        dll.mpv_free.argtypes = [ctypes.c_void_p]
        dll.mpv_free.restype = None
        dll.mpv_terminate_destroy.argtypes = [ctypes.c_void_p]
        dll.mpv_terminate_destroy.restype = None
        self.handle = dll.mpv_create()
        if not self.handle:
            raise RuntimeError("mpv_create returned null")
        try:
            for name, value in [
                ("config", "no"), ("vo", "null"), ("ao", "null"),
                ("ytdl", "no"), ("terminal", "no"), ("hwdec", "no"),
                ("keep-open", "yes"), ("audio-display", "no"),
            ]:
                rc = dll.mpv_set_option_string(self.handle, name.encode(),
                                               value.encode())
                if rc < 0:
                    raise RuntimeError("mpv option %s rejected rc=%d" % (name, rc))
            rc = dll.mpv_initialize(self.handle)
            if rc < 0:
                raise RuntimeError("mpv_initialize rc=%d" % rc)
        except BaseException:
            dll.mpv_terminate_destroy(self.handle)
            self.handle = None
            raise

    def close(self):
        if self.handle:
            h, self.handle = self.handle, None
            self.lib.mpv_terminate_destroy(h)

    def command(self, *args):
        data = (ctypes.c_char_p * (len(args) + 1))()
        for i, arg in enumerate(args):
            data[i] = arg.encode("utf-8")
        data[-1] = None
        rc = self.lib.mpv_command(self.handle, data)
        return rc

    def int_property(self, name):
        out = ctypes.c_int64()
        rc = self.lib.mpv_get_property(self.handle, name.encode(),
                                       MPV_FORMAT_INT64, ctypes.byref(out))
        return int(out.value) if rc >= 0 else None

    def flag_property(self, name):
        out = ctypes.c_int()
        rc = self.lib.mpv_get_property(self.handle, name.encode(),
                                       MPV_FORMAT_FLAG, ctypes.byref(out))
        return bool(out.value) if rc >= 0 else None

    def string_property(self, name):
        out = ctypes.c_void_p()
        rc = self.lib.mpv_get_property(self.handle, name.encode(),
                                       MPV_FORMAT_STRING, ctypes.byref(out))
        if rc < 0 or not out.value:
            return None
        try:
            return ctypes.string_at(out.value).decode("utf-8", errors="replace")
        finally:
            self.lib.mpv_free(out)

    def tracks(self):
        count = self.int_property("track-list/count")
        if count is None or count < 0 or count > 64:
            return []
        result = []
        for i in range(count):
            base = "track-list/%d/" % i
            role = self.string_property(base + "type")
            if role not in ("audio", "video", "sub"):
                continue
            result.append({"role": role, "selected": self.flag_property(
                base + "selected"), "external": self.flag_property(
                base + "external")})
        return result

    def until(self, condition, seconds=8):
        deadline = time.monotonic() + seconds
        observed = []
        while time.monotonic() < deadline:
            event = self.lib.mpv_wait_event(self.handle, 0.10)
            if event:
                event_id = event.contents.event_id
                if event_id != 0:
                    observed.append(event_id)
                if event_id == QUEUE_OVERFLOW:
                    raise AssertionError("LIBMPV_EVENT_OVERFLOW")
                if event_id == SHUTDOWN:
                    raise AssertionError("LIBMPV_UNEXPECTED_SHUTDOWN")
            if condition(observed):
                return observed
        raise AssertionError("LIBMPV_OBSERVATION_TIMEOUT observed=%s" %
                             sorted(set(observed)))


def selected_track(tracks, role):
    return any(t["role"] == role and t["selected"] is True for t in tracks)


def exercise(lib, base, source):
    # No device playback; media decode and track selection are real libmpv APIs.
    mpv = RealMpv(lib)
    try:
        rc = mpv.command("loadfile", base + "/video", "replace")
        if rc < 0:
            raise AssertionError("MPV_VIDEO_LOADFILE_COMMAND_REJECTED rc=%d" % rc)
        mpv.until(lambda e: FILE_LOADED in e and selected_track(mpv.tracks(), "video"))
        video_pts_before = mpv.int_property("time-pos")
        rc = mpv.command("audio-add", base + "/audio", "select")
        if rc < 0:
            raise AssertionError("MPV_AUDIO_ADD_COMMAND_REJECTED rc=%d" % rc)
        mpv.until(lambda _e: selected_track(mpv.tracks(), "audio"))
        records = source.snapshot()
        audio_records = [r for r in records if r["role"] == "audio"]
        video_records = [r for r in records if r["role"] == "video"]
        if not audio_records or not video_records:
            raise AssertionError("MISSING_DISTINCT_AUDIO_VIDEO_HTTP_RECEIPTS")
        if any(r["status"] != 200 or r["bytes_sent"] != r["declared_bytes"]
               for r in audio_records + video_records):
            raise AssertionError("HEALTHY_HTTP_RECEIPT_INCOMPLETE")
        tracks = mpv.tracks()
        print("I42_REAL_LIBMPV_AV_CONTROL_PASS " + json.dumps({
            "video_requests": len(video_records), "audio_requests": len(audio_records),
            "selected_video": selected_track(tracks, "video"),
            "selected_audio": selected_track(tracks, "audio"),
            "external_audio": any(t["role"] == "audio" and t["external"] is True
                                  for t in tracks),
            "clock_domain": "mpv-player", "video_pts_before": video_pts_before,
            "output": "null", "audible": False, "http_ownership": "fixture-only",
        }, sort_keys=True), flush=True)
    finally:
        mpv.close()


def check_audio_503(lib, base, source):
    source.set_modes(audio="503")
    mpv = RealMpv(lib)
    try:
        rc = mpv.command("loadfile", base + "/video", "replace")
        if rc < 0:
            raise AssertionError("MPV_VIDEO_LOADFILE_COMMAND_REJECTED rc=%d" % rc)
        mpv.until(lambda e: FILE_LOADED in e and selected_track(mpv.tracks(), "video"))
        rc = mpv.command("audio-add", base + "/audio", "select")
        if rc < 0:
            raise AssertionError("MPV_AUDIO_FAIL_COMMAND_REJECTED rc=%d" % rc)
        mpv.until(lambda _: any(r["role"] == "audio" and r["status"] == 503
                                for r in source.snapshot()))
        tracks = mpv.tracks()
        if not selected_track(tracks, "video"):
            raise AssertionError("VIDEO_TRACK_LOST_WHEN_AUDIO_HTTP_503")
        failures = [r for r in source.snapshot() if r["role"] == "audio"
                    and r["status"] == 503]
        if not failures:
            raise AssertionError("LIBMPV_AUDIO_HTTP_503_NOT_REPRODUCED")
        print("I42_REAL_LIBMPV_AUDIO_503_OBSERVED " + json.dumps({
            "audio_503_receipts": len(failures),
            "video_track_selected": selected_track(tracks, "video"),
            "audio_track_selected": selected_track(tracks, "audio"),
            "mpv_ingest_verified": False,
            "app_pcm_verified": False,
            "source_to_selected_track_bound": False,
            "first_broken_boundary": "UNKNOWN",
            "audible_output_verified": False,
        }, sort_keys=True), flush=True)
    finally:
        mpv.close()


def main():
    if sys.platform != "win32":
        raise SystemExit("I42_REAL_LIBMPV requires pinned Windows DLL")
    root = Path(__file__).resolve().parents[1]
    dllpath = root / "deps" / "mpv" / "libmpv-2.dll"
    if not dllpath.is_file():
        raise SystemExit("I42_REAL_LIBMPV_DLL_MISSING")
    source = Source()
    with os.add_dll_directory(str(dllpath.parent)):
        dll = ctypes.WinDLL(str(dllpath))
        with localhost_fixture(source) as base:
            exercise(dll, base, source)
        # A new route owner + server for the failure, without reusing source ID.
        fault_source = Source()
        with localhost_fixture(fault_source) as base:
            check_audio_503(dll, base, fault_source)
    print("I42_REAL_LIBMPV_HTTP_FIXTURE_PASS physical_audio=NOT_RUN "
          "native_app=NOT_RUN source_root=UNKNOWN", flush=True)


if __name__ == "__main__":
    main()
