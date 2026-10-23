"""Issue #42 N00–N16: loopback-only independent audio/video HTTP RED-first.

No mpv or app is launched. The HTTP server proves only the controlled stimulus.
Native/source assertions at the end are intentionally semantic RED: current
diagnostics cannot identify a separately loaded audio URL. A synthetic trace
must never attest root cause, audible output, or source reachability.
"""
import contextlib
import http.client
import pathlib
import socket
import sys
import threading
import time
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from issue42_first_boundary import analyze

AUDIO = bytes([31, 61, 91, 121]) * 1024
VIDEO = bytes([24, 48, 72, 96]) * 1024
SOURCE = ROOT / "native/src/player/event/PlaybackObserver.cpp"


class FaultSite:
    """Resource-role indexed faults; receipts contain no URL or private token."""
    def __init__(self):
        self.faults = {}
        self.records = []
        self.lock = threading.Lock()

    def set_fault(self, role, seg, kind):
        assert role in ("audio", "video")
        with self.lock:
            self.faults[(role, seg)] = kind

    def clear_fault(self, role, seg):
        with self.lock:
            self.faults.pop((role, seg), None)

    def receipt(self, role, seg):
        with self.lock:
            kind = self.faults.get((role, seg), "ok")
            request_id = len(self.records) + 1
            row = {"request_id": request_id, "role": role, "segment": seg,
                   "kind": kind, "mono_ns": time.monotonic_ns()}
            self.records.append(row)
            return row


@contextlib.contextmanager
def site():
    fixture = FaultSite()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, _format, *_args):
            return  # Never put query strings, credentials, or URLs in CI logs.

        def do_GET(self):
            parts = self.path.split("?")[0].strip("/").split("/")
            if (len(parts) != 2 or parts[0] not in ("audio", "video") or
                    not parts[1].isdigit() or len(parts[1]) > 5):
                self.send_error(404)
                return
            role, seg = parts[0], int(parts[1])
            receipt = fixture.receipt(role, seg)
            kind = receipt["kind"]
            payload = AUDIO if role == "audio" else VIDEO
            if kind == "zero_pcm":
                payload = bytes(len(AUDIO))
            if kind == "stall":
                time.sleep(0.12)
            if kind == "reset":
                self.close_connection = True
                with contextlib.suppress(OSError):
                    self.connection.shutdown(socket.SHUT_RDWR)
                return
            if kind.startswith("status:"):
                status = int(kind.split(":", 1)[1])
                self.send_response(status)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            if kind == "range_bad":
                self.send_response(206)
                self.send_header("Content-Range", "bytes 500-510/50")
            else:
                self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(
                len(payload) + (256 if kind == "truncate" else 0)))
            self.end_headers()
            try:
                self.wfile.write(payload[:64] if kind == "truncate" else payload)
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                pass
            if kind == "truncate":
                self.close_connection = True

    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    srv.daemon_threads = True
    srv.timeout = 0.5
    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        yield fixture, "http://127.0.0.1:%d" % srv.server_port
    finally:
        srv.shutdown()
        srv.server_close()
        thread.join(timeout=3)
        if thread.is_alive():
            raise AssertionError("LOOPBACK_FIXTURE_CLEANUP_FAILED")


def get(base, role, seg):
    assert role in ("audio", "video") and isinstance(seg, int)
    request = urllib.request.Request("%s/%s/%d" % (base, role, seg))
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    start = time.monotonic()
    try:
        with opener.open(request, timeout=3) as resp:
            body = resp.read()
            return (resp.status, body, None,
                    resp.headers.get("Content-Range"),
                    time.monotonic() - start)
    except urllib.error.HTTPError as err:
        err.close()
        return (err.code, b"", None, None, time.monotonic() - start)
    except (http.client.IncompleteRead, urllib.error.URLError, OSError) as err:
        return (None, b"", type(err).__name__, None, time.monotonic() - start)


def marker(stage, when, *, load=1, generation=0, run=7, session=3, **extra):
    pairs = {"stage": stage, "t_us": when, "load_id": load,
             "generation": generation, "run_uid": run, "session_uid": session}
    pairs.update(extra)
    return "[AUDIO-TELEMETRY] " + " ".join("%s=%s" % item for item in pairs.items())


def underflow(when, **extra):
    fields = {"paused": 0, "core_idle": 0, "idle_active": 0,
              "seeking": 0, "pause_for_cache": 0}
    fields.update(extra)
    return marker("SDL_QUEUE_UNDERFLOW", when, **fields)


class IndependentHttpAVControls(unittest.TestCase):
    """N00–N16 are actual HTTP transport/control oracles, not media playback."""

    def test_n00_local_muxed_no_network_control(self):
        self.assertTrue(AUDIO and VIDEO)
        self.assertEqual(analyze("")["classification"], "NO_UNDERFLOW_OBSERVED")
        self.assertEqual(analyze("")["root_cause"], "UNKNOWN")

    def test_n01_distinct_healthy_audio_video(self):
        with site() as (fx, base):
            audio, video = get(base, "audio", 0), get(base, "video", 0)
            self.assertEqual((audio[0], video[0]), (200, 200))
            self.assertEqual((audio[1], video[1]), (AUDIO, VIDEO))
            self.assertEqual([r["role"] for r in fx.records], ["audio", "video"])

    def test_n02_cold_audio_404_video_healthy(self):
        with site() as (fx, base):
            fx.set_fault("audio", 0, "status:404")
            self.assertEqual(get(base, "audio", 0)[0], 404)
            self.assertEqual(get(base, "video", 0)[0], 200)
            self.assertEqual(len([r for r in fx.records if r["role"] == "audio"]), 1)

    def test_n03_audio_segment_503_after_healthy_prefix(self):
        with site() as (fx, base):
            self.assertEqual(get(base, "audio", 0)[0], 200)
            self.assertEqual(get(base, "video", 0)[0], 200)
            fx.set_fault("audio", 1, "status:503")
            self.assertEqual(get(base, "audio", 1)[0], 503)
            self.assertEqual(get(base, "video", 1)[0], 200)
            self.assertNotIn("status:503", [r["kind"] for r in fx.records
                                            if r["role"] == "video"])

    def test_n04_audio_stall_without_http_error(self):
        with site() as (fx, base):
            fx.set_fault("audio", 2, "stall")
            response = get(base, "audio", 2)
            self.assertEqual(response[:2], (200, AUDIO))
            self.assertGreaterEqual(response[4], 0.09)
            self.assertEqual(get(base, "video", 2)[0], 200)

    def test_n05_truncated_200_is_not_complete_success(self):
        with site() as (fx, base):
            fx.set_fault("audio", 3, "truncate")
            self.assertEqual(get(base, "video", 3)[0], 200)
            result = get(base, "audio", 3)
            self.assertEqual(result[2], "IncompleteRead")

    def test_n06_audio_connection_reset_does_not_fail_video(self):
        with site() as (fx, base):
            fx.set_fault("audio", 4, "reset")
            self.assertIsNotNone(get(base, "audio", 4)[2])
            self.assertEqual(get(base, "video", 4)[0], 200)

    def test_n07_audio_retry_recovery_is_explicit(self):
        with site() as (fx, base):
            fx.set_fault("audio", 5, "status:503")
            self.assertEqual(get(base, "audio", 5)[0], 503)
            fx.clear_fault("audio", 5)
            self.assertEqual(get(base, "audio", 5)[:2], (200, AUDIO))
            receipts = [r for r in fx.records if r["role"] == "audio"]
            self.assertEqual([r["kind"] for r in receipts], ["status:503", "ok"])
            self.assertLess(receipts[0]["request_id"], receipts[1]["request_id"])

    def test_n08_video_only_failure_not_audio_failure(self):
        with site() as (fx, base):
            fx.set_fault("video", 6, "status:503")
            self.assertEqual(get(base, "video", 6)[0], 503)
            self.assertEqual(get(base, "audio", 6)[:2], (200, AUDIO))

    def test_n09_simultaneous_fault_is_not_isolated_audio(self):
        with site() as (fx, base):
            fx.set_fault("audio", 7, "status:503")
            fx.set_fault("video", 7, "status:503")
            self.assertEqual(get(base, "audio", 7)[0], 503)
            self.assertEqual(get(base, "video", 7)[0], 503)
            self.assertEqual({r["role"] for r in fx.records}, {"audio", "video"})

    def test_n10_healthy_http_but_processor_drop(self):
        with site() as (_fx, base):
            self.assertEqual(get(base, "audio", 8)[0], 200)
            log = "\n".join([marker("PROCESSOR_PUBLISH_DROP", 1200),
                             underflow(1500, last_generation=0)])
            result = analyze(log)
            self.assertEqual(result["classification"], "PROCESSOR_DROP_OBSERVED")
            self.assertEqual(result["root_cause"], "UNKNOWN")

    def test_n11_healthy_http_but_output_device_drop(self):
        with site() as (_fx, base):
            self.assertEqual(get(base, "audio", 8)[0], 200)
            log = "\n".join([marker("OUTPUT_DEVICE_OFFLINE_DROP", 1200),
                             underflow(1500, last_generation=0)])
            result = analyze(log)
            self.assertEqual(result["classification"],
                             "OUTPUT_DEVICE_OFFLINE_DROP_OBSERVED")
            self.assertEqual(result["root_cause"], "UNKNOWN")

    def test_n12_old_generation_never_explains_new_underflow(self):
        log = "\n".join([marker("PROCESSOR_PUBLISH_DROP", 1200, generation=0),
                         underflow(1500, generation=1)])
        self.assertEqual(analyze(log)["classification"], "INCONCLUSIVE")

    def test_n13_seek_segment_refetch_is_not_automatic_retry(self):
        with site() as (fx, base):
            self.assertEqual(get(base, "audio", 0)[0], 200)
            fx.set_fault("audio", 1, "status:503")
            self.assertEqual(get(base, "audio", 1)[0], 503)
            self.assertEqual(get(base, "audio", 0)[0], 200)
            self.assertEqual([r["segment"] for r in fx.records], [0, 1, 0])

    def test_n14_cross_session_same_load_no_borrow(self):
        log = "\n".join([marker("PROCESSOR_PUBLISH_DROP", 1200, run=7, session=3),
                         underflow(1500, run=8, session=4)])
        self.assertEqual(analyze(log)["classification"], "INCONCLUSIVE")

    def test_n15_shutdown_underflow_never_verified_playback(self):
        log = "\n".join(["[Lifecycle] component=PlayerSession phase=STOP id=s1",
                         underflow(1500)])
        result = analyze(log)
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertFalse(result["playback_context_verified"])

    def test_n16_zero_samples_are_not_no_pcm(self):
        with site() as (fx, base):
            fx.set_fault("audio", 9, "zero_pcm")
            status, data, error, *_ = get(base, "audio", 9)
            self.assertEqual(status, 200)
            self.assertIsNone(error)
            self.assertEqual(len(data), len(AUDIO))
            self.assertEqual(set(data), {0})

    def test_range_206_mismatch_is_not_good_audio(self):
        with site() as (fx, base):
            fx.set_fault("audio", 10, "range_bad")
            status, data, error, content_range, *_ = get(base, "audio", 10)
            self.assertEqual(status, 206)
            self.assertIsNone(error)
            self.assertTrue(content_range.endswith("/50"))
            self.assertGreater(len(data), 50)


class MissingExternalAudioNativeEvidenceRED(unittest.TestCase):
    """Target semantic RED, not harness, because native external-URL binding is absent."""

    @classmethod
    def setUpClass(cls):
        cls.native = SOURCE.read_text(encoding="utf-8")

    def test_or01_external_audio_is_not_main_demuxer_cache(self):
        self.assertIn("source_scope=main_demuxer", self.native)  # frozen control
        self.assertIn("source_reachability=unverified", self.native)
        self.assertIn("stage=URL_AUDIO_EXTERNAL_SOURCE_STATE", self.native,
                      "RED: no external-audio source-status event distinct from main demuxer")

    def test_or02_http_error_must_have_independent_audio_attribution(self):
        self.assertIn("component=unattributed", self.native)  # existing safe marker
        self.assertIn("stage=URL_AUDIO_TRACK_SOURCE_FAILURE", self.native,
                      "RED: no corroborated per-track audio source error evidence")

    def test_or03_audio_source_evidence_needs_load_session_lineage(self):
        self.assertIn("stage=URL_AUDIO_STATE", self.native)
        self.assertIn("stage=URL_AUDIO_SOURCE_CORRELATION", self.native,
                      "RED: source evidence has no explicit run/session/load correlation")

    def test_or04_external_cache_unknown_not_main_cache_health(self):
        self.assertIn("stage=URL_AUDIO_STREAM_CACHE", self.native)
        self.assertIn("external_audio_cache_known=", self.native,
                      "RED: external audio cache remains UNKNOWN rather than main cache")


if __name__ == "__main__":
    unittest.main()
