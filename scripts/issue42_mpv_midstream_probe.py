"""Issue #42 MID01: audio-only late HLS segment failure with real pinned libmpv.

Native hosted-Windows, local transport. The first AAC HLS segments succeed.
A later audio segment stays HELD (no response) until libmpv has selected
external audio and video media clock progresses, then is answered 503.
Do not claim actual Im_player PCM/SDL, audible loss, source HTTP ownership,
or first broken audio boundary from this fixture.
"""
import base64
import ctypes
import json
import os
from pathlib import Path
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from issue42_libmpv_http_probe import (
    RealMpv, y4m_bytes, selected_track, FILE_LOADED, QUEUE_OVERFLOW,
)

AAC_ADTS_B64 = "//FMQArf/N4CAExhdmM2MS4xOS4xMDEAAqyaFfREFvvnXt1xqXd3JIkkb06EECekimi41fL5fKb5cflx+Xy+WX5cePHjFEUUUUUUUUUUUUURRRRRRRf/8UxAC3/8ATiM2spaI2iVspV/XF6/8a+/XF//HH344v++/v51odLxq0EhISEv/CXmt7YeOhCcNU3nm/nMeeHOKt/ABuV4hUb5KN8lD5Tj5cZwA/7VAuY4cCZu//FMQAef/AEu7Qr6M/p/0/X4111Jw4rirtcjnQZ+Y6y9/fQT/w2HfcB/4Az+4D/w2HfcDJ8AO+4GT4HA//FMQAe//AEELQb6N9v+3/0/f/93Xnq5eRU35787t27sWta0NWrVqp/M2qdIL0Wv5TJOk7U2RRn2Q3w+fP/xTEAHf/wBBC0G+jfb/8P/H7//u69taa5mlb435w7dwWtS0NWrVDBvvTet3eX+t8nZY7TWUXW2ZQ38//FMQAf//AECLQr6N9v/7P9/3//d1561VskrfXfts78sYYHWURRFc0ev16H09VbWEazIAF4Uekums6zgdYg4//FMQAff/AEGLQr6N7//2/6/v/+7z1xK4wZOO+Kt2c2MMD3ly5RRKWF1q9d3Qpo6gqcDo1rC1QnBmlgJNLz/8UxAB5/8AQgtCeKNG+f/7f7fz/+ft17Uq96Zz7FR17A5z3lddpuuoMFzrVQZq17LFLU41bol6n7R/Dj/8UxAB5/8AQgtCkJdG+f/7n7f7f/bz7cLN6rjKrVU6+dA5zsJ55542vCg55wZqg4HjpnMs4747hexQ4D/8UxAB7/8AQgtBvo32//t/t/7f+Xt5462pNVzrOqp2ZYpSk4Tnr1znG1qW/aPayn4bKiy1u7ZHZXkdmTg//FMQAcf/AEILQb6N9v/7f7f+3/l7dea0bkVecMde4KUpOM9ev4YXS0aRoWsX8osvWSjtPsXduD/8UxAB//8AUpNEgJ9G79/t+Pj2++TidVa6uWjasAARZks+2kAABi0PPrEABhx4e2+f9DP/iLb98zP/gf0JcD/8UxACX/8ATKNpsjbKWylDN/jqX/bPv1rX8f141LEoGdU7E1Bc3/q6PubP5tl0bg5/6NlMb9XkcaCSoSEhJwJayqXsA8GBgYGBnD/8UxAAZ/8ARiBtHA="
SEGMENT_COUNT = 12
FAIL_SEGMENT = 6
MPV_FORMAT_DOUBLE = 5


def media_time(mpv):
    value = ctypes.c_double()
    rc = mpv.lib.mpv_get_property(
        mpv.handle, b"time-pos", MPV_FORMAT_DOUBLE, ctypes.byref(value))
    return value.value if rc >= 0 else None


class SegmentedFixture:
    def __init__(self):
        self.video = y4m_bytes()
        self.audio_piece = base64.b64decode(AAC_ADTS_B64, validate=True) * 4
        if len(self.audio_piece) < 1000:
            raise AssertionError("MID01_AUDIO_SEGMENT_INVALID")
        self.manifest = (
            "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:2\n"
            "#EXT-X-MEDIA-SEQUENCE:0\n"
            + "".join("#EXTINF:1.109,\nseg%d.aac\n" % n
                      for n in range(SEGMENT_COUNT))
            + "#EXT-X-ENDLIST\n"
        ).encode("ascii")
        self.fault_requested = threading.Event()
        self.allow_fault = threading.Event()
        self.fault_response_sent = threading.Event()
        self.video_complete = threading.Event()
        self.lock = threading.Lock()
        self.records = []

    def record(self, kind, *, segment=-1, status=200, bytes_sent=0):
        with self.lock:
            self.records.append(dict(kind=kind, segment=segment, status=status,
                                     bytes_sent=bytes_sent))

    def snapshot(self):
        with self.lock:
            return list(self.records)


def experiment(lib):
    fx = SegmentedFixture()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass  # Never log request headers, URLs or error text.

        def do_GET(self):
            path = self.path  # Exact allowlist; query strings are forbidden.
            if path == "/video":
                kind, content_type, data, seg = "video", "video/x-yuv4mpeg", fx.video, -1
            elif path == "/audio.m3u8":
                kind, content_type, data, seg = "manifest", "application/vnd.apple.mpegurl", fx.manifest, -1
            elif path.startswith("/seg") and path.endswith(".aac") and path[4:-4].isdigit():
                seg = int(path[4:-4])
                if not 0 <= seg < SEGMENT_COUNT:
                    self.send_error(404)
                    return
                kind, content_type, data = "audio_segment", "audio/aac", fx.audio_piece
            else:
                self.send_error(404)
                return

            if kind == "audio_segment" and seg == FAIL_SEGMENT:
                fx.fault_requested.set()
                if not fx.allow_fault.wait(9):
                    fx.record("fault_wait_timeout", segment=seg, status=0)
                    self.close_connection = True
                    return
                self.send_response(503)
                self.send_header("Content-Length", "0")
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                fx.record("audio_segment", segment=seg, status=503)
                fx.fault_response_sent.set()
                return

            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            try:
                self.wfile.write(data)
                self.wfile.flush()
                fx.record(kind, segment=seg, status=200, bytes_sent=len(data))
                if kind == "video":
                    fx.video_complete.set()
            except OSError:
                fx.record(kind, segment=seg, status=200, bytes_sent=0)

    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    srv.daemon_threads = True
    srv_thread = threading.Thread(target=srv.serve_forever, daemon=True)
    srv_thread.start()
    base = "http://127.0.0.1:%d" % srv.server_port
    mpv = None
    worker = None
    call_status = {}
    try:
        mpv = RealMpv(lib)
        if mpv.command("loadfile", base + "/video", "replace") < 0:
            raise AssertionError("MID01_VIDEO_LOAD_FAILED")
        mpv.until(lambda events: FILE_LOADED in events and
                  selected_track(mpv.tracks(), "video"))
        baseline = media_time(mpv)
        if baseline is None:
            raise AssertionError("MID01_MEDIA_CLOCK_UNAVAILABLE")

        def call_audio_add():
            try:
                call_status["rc"] = mpv.command("audio-add", base + "/audio.m3u8", "select")
            except BaseException as ex:
                call_status["exception"] = type(ex).__name__

        worker = threading.Thread(target=call_audio_add, daemon=True)
        worker.start()
        start = time.monotonic()
        selected = False
        external = False
        video_selected = False
        video_progressed = False
        delta = 0.0
        while time.monotonic() - start < 7.0:
            event = mpv.lib.mpv_wait_event(mpv.handle, 0.0)
            if event and event.contents.event_id == QUEUE_OVERFLOW:
                raise AssertionError("MID01_NATIVE_EVENT_OVERFLOW")
            tracks = mpv.tracks()
            selected = selected_track(tracks, "audio")
            external = any(t["role"] == "audio" and
                           t["selected"] is True and
                           t["external"] is True for t in tracks)
            video_selected = selected_track(tracks, "video")
            pts = media_time(mpv)
            delta = pts - baseline if pts is not None else 0.0
            video_progressed = delta >= 0.35
            records = fx.snapshot()
            healthy = len({r["segment"] for r in records
                           if r["kind"] == "audio_segment"
                           and 0 <= r["segment"] < FAIL_SEGMENT
                           and r["status"] == 200 and r["bytes_sent"] > 1000}) >= 2
            if fx.fault_requested.is_set() and healthy and selected and external and video_selected and video_progressed and fx.video_complete.is_set():
                break
            time.sleep(0.09)

        records = fx.snapshot()
        healthy = len({r["segment"] for r in records
                       if r["kind"] == "audio_segment" and
                       0 <= r["segment"] < FAIL_SEGMENT and
                       r["status"] == 200 and r["bytes_sent"] > 1000}) >= 2
        ready = (fx.video_complete.is_set() and healthy and
                 fx.fault_requested.is_set() and selected and external and
                 video_selected and video_progressed)
        if not ready:
            print("I42_MID01_SEMANTIC_RED " + json.dumps({
                "video_http_complete": fx.video_complete.is_set(),
                "healthy_audio_segments": healthy,
                "late_segment_request_held": fx.fault_requested.is_set(),
                "selected_audio": selected, "selected_external_audio": external,
                "selected_video": video_selected, "video_clock_progress": video_progressed,
                "video_pts_delta_s": round(delta, 3),
                "audio_add_rc": call_status.get("rc"),
                "root_cause": "UNKNOWN",
            }, sort_keys=True), flush=True)
            raise AssertionError("MID01_PRE_CUT_NATIVE_PROOF_MISSING")

        # The 503 is authorized only after all native in-play preconditions.
        fx.allow_fault.set()
        if not fx.fault_response_sent.wait(3):
            raise AssertionError("MID01_HTTP_503_RECEIPT_NOT_OBSERVED")
        worker.join(timeout=3)
        if worker.is_alive():
            raise AssertionError("MID01_AUDIO_ADD_COMMAND_NOT_FINISHED")
        if "exception" in call_status:
            raise AssertionError("MID01_LIBMPV_CALL_EXCEPTION")
        if not selected_track(mpv.tracks(), "video"):
            raise AssertionError("MID01_VIDEO_TRACK_NOT_SELECTED_AFTER_503")
        post = fx.snapshot()
        errors = [r for r in post if r["kind"] == "audio_segment"
                  and r["segment"] == FAIL_SEGMENT and r["status"] == 503]
        if len(errors) != 1:
            raise AssertionError("MID01_MISSING_UNIQUE_AUDIO_503")

        # MID02 RED-first: video track still being selected after a 503 is
        # not evidence that its timeline continues. Sample a NEW mpv media
        # clock baseline after the independent AUDIO 503 receipt, then require
        # fresh post-fault advancement in this same native libmpv session.
        # This does NOT imply an actual rendered frame or audible audio.
        post_fault_baseline = media_time(mpv)
        if post_fault_baseline is None:
            raise AssertionError("MID02_POST_FAULT_MEDIA_CLOCK_UNKNOWN")
        post_fault_progress_s = 0.0
        observed_post_fault = 0
        post_fault_deadline = time.monotonic() + 3.0
        while time.monotonic() < post_fault_deadline:
            event = mpv.lib.mpv_wait_event(mpv.handle, 0.0)
            if event and event.contents.event_id == QUEUE_OVERFLOW:
                raise AssertionError("MID02_NATIVE_EVENT_OVERFLOW")
            if not selected_track(mpv.tracks(), "video"):
                raise AssertionError("MID02_VIDEO_TRACK_LOST_AFTER_AUDIO_503")
            sample = media_time(mpv)
            if sample is not None:
                observed_post_fault += 1
                post_fault_progress_s = max(
                    post_fault_progress_s, sample - post_fault_baseline)
            if post_fault_progress_s >= 0.30:
                break
            time.sleep(0.09)
        if observed_post_fault < 2 or post_fault_progress_s < 0.30:
            print("I42_MID02_POST_FAULT_VIDEO_PROGRESS_RED " + json.dumps({
                "selected_video_after_audio_error": True,
                "audio_segment_503_receipts": len(errors),
                "post_fault_pts_samples": observed_post_fault,
                "post_fault_pts_advance_s": round(post_fault_progress_s, 3),
                "root_cause": "UNKNOWN",
                "app_pcm_verified": False,
            }, sort_keys=True), flush=True)
            raise AssertionError("MID02_VIDEO_TIMELINE_NOT_PROVEN_AFTER_AUDIO_503")

        print("I42_MIDPLAY_EXTERNAL_AUDIO_SEGMENT_503_OBSERVED " + json.dumps({
            "real_pinned_libmpv": True, "audio_selected_before_fault": True,
            "external_audio_track": True, "video_advanced_pre_fault": True,
            "video_pts_delta_pre_fault_s": round(delta, 3),
            "audio_healthy_segment_count_min": 2,
            "audio_failed_segment_index": FAIL_SEGMENT,
            "failed_audio_route_receipts": len(errors),
            "video_selected_after_audio_fault": True,
            "video_pts_advanced_post_audio_fault": True,
            "video_pts_delta_post_audio_fault_s": round(post_fault_progress_s, 3),
            "audio_add_rc": call_status.get("rc"),
            "mpv_audio_decoder_continuity_verified": False,
            "application_pcm_verified": False, "sdl_verified": False,
            "audible_output_verified": False,
            "root_cause": "UNKNOWN", "first_broken_boundary": "UNKNOWN",
        }, sort_keys=True), flush=True)
    finally:
        fx.allow_fault.set()
        if worker:
            worker.join(timeout=3)
        if mpv:
            mpv.close()
        srv.shutdown()
        srv.server_close()
        srv_thread.join(timeout=3)
        if srv_thread.is_alive():
            raise AssertionError("MID01_SERVER_THREAD_LEAK")


def main():
    if sys.platform != "win32":
        raise SystemExit("MID01_REQUIRES_WINDOWS_PINNED_LIBMPV")
    path = Path(__file__).resolve().parents[1] / "deps/mpv/libmpv-2.dll"
    if not path.is_file():
        raise SystemExit("MID01_PINNED_LIBMPV_MISSING")
    with os.add_dll_directory(str(path.parent)):
        experiment(ctypes.WinDLL(str(path)))
    print("MID01_DONE native_libmpv=true app_pcm=NOT_RUN audible=NOT_RUN", flush=True)


if __name__ == "__main__":
    main()
