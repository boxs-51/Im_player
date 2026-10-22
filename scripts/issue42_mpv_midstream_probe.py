"""Issue42 MID01: real pinned libmpv + owned local HTTP midplay audio stream cut.

Test-only native Windows probe; vo=null/ao=null; NOT physical audible PCM or SDL.
Must see real selected external audio, owned audio HTTP prefix, and media video
clock progression BEFORE cutting only the audio connection. No recovery behavior.
"""
import ctypes
import json
import os
from pathlib import Path
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from issue42_libmpv_http_probe import (
    RealMpv, y4m_bytes, wav_bytes, selected_track, FILE_LOADED, QUEUE_OVERFLOW,
)

PREFIX = 196608
MPV_FORMAT_DOUBLE = 5


class CutFixture:
    def __init__(self):
        self.video = y4m_bytes()
        self.audio = wav_bytes()
        assert len(self.audio) >= PREFIX * 3
        self.prefix_ready = threading.Event()
        self.cut_allowed = threading.Event()
        self.cut_done = threading.Event()
        self.video_done = threading.Event()
        self.receipts = []
        self.lock = threading.Lock()

    def receipt(self, role, status, sent, declared, phase):
        with self.lock:
            self.receipts.append(dict(role=role, status=status, sent=sent,
                                      declared=declared, phase=phase))

    def snapshot(self):
        with self.lock:
            return list(self.receipts)


def video_pts(mpv):
    value = ctypes.c_double()
    rc = mpv.lib.mpv_get_property(
        mpv.handle, b"time-pos", MPV_FORMAT_DOUBLE, ctypes.byref(value))
    return value.value if rc >= 0 else None


def scenario(lib):
    fx = CutFixture()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            if self.path not in ("/audio", "/video"):
                self.send_error(404)
                return
            role = self.path[1:]
            blob = fx.audio if role == "audio" else fx.video
            self.send_response(200)
            self.send_header("Content-Length", str(len(blob)))
            self.send_header("Content-Type", "audio/wav" if role == "audio"
                             else "video/x-yuv4mpeg")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            try:
                if role == "video":
                    self.wfile.write(blob)
                    self.wfile.flush()
                    fx.receipt(role, 200, len(blob), len(blob), "COMPLETE")
                    fx.video_done.set()
                    return
                self.wfile.write(blob[:PREFIX])
                self.wfile.flush()
                fx.receipt(role, 200, PREFIX, len(blob), "PREFIX_DELIVERED")
                fx.prefix_ready.set()
                released = fx.cut_allowed.wait(6)
                if not released:
                    fx.receipt(role, 200, PREFIX, len(blob), "TIMEOUT_NOT_AUTHORIZED")
            except OSError:
                fx.receipt(role, 200, 0, len(blob), "CLIENT_CLOSED")
            finally:
                if role == "audio":
                    self.close_connection = True
                    try:
                        self.connection.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
                    fx.cut_done.set()

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    base = "http://127.0.0.1:%d" % server.server_port
    mpv = None
    add_thread = None
    result = {}
    try:
        mpv = RealMpv(lib)
        if mpv.command("loadfile", base + "/video", "replace") < 0:
            raise AssertionError("MID01_VIDEO_LOAD_REJECTED")
        mpv.until(lambda ev: FILE_LOADED in ev and selected_track(mpv.tracks(), "video"))
        pts0 = video_pts(mpv)
        if pts0 is None:
            raise AssertionError("MID01_VIDEO_CLOCK_UNKNOWN")

        def call_audio():
            try:
                result["rc"] = mpv.command("audio-add", base + "/audio", "select")
            except BaseException as ex:
                result["error"] = type(ex).__name__

        # Isolate possibly blocking synchronous audio-add from the controller.
        add_thread = threading.Thread(target=call_audio, daemon=True)
        add_thread.start()
        started = time.monotonic()
        selected = False
        progressed = False
        video_ok = False
        external = False
        delta = 0.0
        while time.monotonic() - started < 4.0:
            event = mpv.lib.mpv_wait_event(mpv.handle, 0)
            if event and event.contents.event_id == QUEUE_OVERFLOW:
                raise AssertionError("MID01_LIBMPV_EVENT_OVERFLOW")
            tracks = mpv.tracks()
            video_ok = selected_track(tracks, "video")
            selected = selected_track(tracks, "audio")
            external = any(t["role"] == "audio" and t["selected"] is True
                           and t["external"] is True for t in tracks)
            now = video_pts(mpv)
            delta = now - pts0 if now is not None else 0.0
            progressed = delta >= 0.35
            if fx.video_done.is_set() and fx.prefix_ready.is_set() and video_ok and external and progressed:
                break
            time.sleep(0.08)

        receipts = fx.snapshot()
        partial = any(r["role"] == "audio" and r["phase"] == "PREFIX_DELIVERED"
                      and r["sent"] == PREFIX and r["declared"] > 3 * PREFIX
                      for r in receipts)
        if not (fx.video_done.is_set() and partial and video_ok and selected
                and external and progressed):
            print("I42_MID01_SEMANTIC_RED " + json.dumps({
                "video_http_complete": fx.video_done.is_set(),
                "audio_http_prefix": partial,
                "audio_selected": selected, "external_audio": external,
                "video_selected": video_ok, "video_pts_advanced": progressed,
                "video_pts_delta_s": round(delta, 3),
                "audio_add_rc": result.get("rc"),
                "root_cause": "UNKNOWN",
            }, sort_keys=True), flush=True)
            raise AssertionError("MID01_PRE_CUT_NATIVE_PROOF_MISSING")

        fx.cut_allowed.set()
        if not fx.cut_done.wait(3):
            raise AssertionError("MID01_AUDIO_SOCKET_CUT_NOT_OBSERVED")
        add_thread.join(timeout=3)
        if add_thread.is_alive():
            raise AssertionError("MID01_AUDIO_ADD_BLOCKED_AFTER_SOCKET_CUT")
        if result.get("error"):
            raise AssertionError("MID01_LIBMPV_CALL_EXCEPTION")
        if not selected_track(mpv.tracks(), "video"):
            raise AssertionError("MID01_VIDEO_TRACK_LOST_AFTER_AUDIO_CUT")

        print("I42_MIDSTREAM_AUDIO_SOCKET_CUT_OBSERVED " + json.dumps({
            "real_pinned_libmpv": True,
            "video_advanced_before_audio_cut": progressed,
            "video_pts_delta_before_cut_s": round(delta, 3),
            "selected_external_audio_before_cut": external,
            "audio_http_bytes_sent": PREFIX,
            "audio_http_declared_bytes": len(fx.audio),
            "audio_body_incomplete": True,
            "video_track_still_selected": True,
            "audio_add_rc": result.get("rc"),
            "app_pcm_verified": False, "sdl_verified": False,
            "audible_output_verified": False,
            "first_broken_boundary": "UNKNOWN", "root_cause": "UNKNOWN",
        }, sort_keys=True), flush=True)
    finally:
        fx.cut_allowed.set()
        if add_thread:
            add_thread.join(timeout=3)
        if mpv:
            mpv.close()
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=3)
        if server_thread.is_alive():
            raise AssertionError("MID01_SERVER_CLEANUP_FAILED")


def main():
    if sys.platform != "win32":
        raise SystemExit("MID01_PINNED_WINDOWS_ONLY")
    dll_path = Path(__file__).resolve().parents[1] / "deps/mpv/libmpv-2.dll"
    if not dll_path.is_file():
        raise SystemExit("MID01_PINNED_LIBMPV_MISSING")
    with os.add_dll_directory(str(dll_path.parent)):
        scenario(ctypes.WinDLL(str(dll_path)))
    print("MID01_DONE real_libmpv=true production_app=NOT_RUN audible=NOT_RUN",
          flush=True)


if __name__ == "__main__":
    main()
