"""Issue #42 BV-T1: controlled localhost audio HTTP/MPV-ownership contract.

TESTS ONLY. The selection and HTTP binding are fixture-owned, not evidence
that native libmpv actually loaded that resource. Missing future v3 reducer
is a defined fail-closed semantic RED, NOT an import or infrastructure error.
No URL, secret, or raw human mpv log may establish audio attribution.
"""
import importlib
import json
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from test_issue42_audio_video_http_redfirst import site, get, AUDIO, VIDEO

try:
    v3 = importlib.import_module("issue42_external_source_v3")
except ModuleNotFoundError as ex:
    if ex.name != "issue42_external_source_v3":
        raise  # A missing dependency inside an existing verifier is HARNESS_ERROR.
    v3 = None

UNKNOWN = "UNKNOWN"
INCONCLUSIVE = "INCONCLUSIVE"


def classify(events):
    candidate = getattr(v3, "assess_owned_audio_transport", None)
    if candidate is None:
        return dict(attribution=INCONCLUSIVE, root_cause=UNKNOWN,
                    audible_output_verified=False, mpv_ingest_verified=False,
                    external_audio_cache_known=False, first_broken_boundary=UNKNOWN)
    return candidate(events)


def selected(*, role="AUDIO", session=3, epoch=1, phase="IN_PLAY"):
    return dict(schema="IM_PLAYER_AUDIO_SOURCE_V3", event_type="TRACK_SELECTED",
                run_uid=7, session_uid=session, load_id=11, retry_attempt=0,
                selection_epoch=epoch, source_uid="opaque-source-1",
                route_uid="opaque-route-1", role=role, track_id=2, selected=True,
                playback_phase=phase, clock_domain="process-steady",
                emitter_id="fixture-track-owner", emitter_seq=1,
                lifecycle_epoch=1, owner_fence_seq=1)


def http_event(source, observed, *, status, body_bytes=0, complete=False):
    return dict(**{k:source[k] for k in (
        "schema", "run_uid", "session_uid", "load_id", "retry_attempt",
        "selection_epoch", "source_uid", "route_uid", "lifecycle_epoch",
        "owner_fence_seq", "playback_phase")},
        event_type="HTTP_REQUEST_OBSERVED", role=observed["role"].upper(),
        request_uid=observed["request_id"], status=status,
        received_bytes=body_bytes, complete=complete,
        clock_domain="http-server-monotonic-UNBOUND",
        emitter_id="fixture-http-owner",
        emitter_seq=observed["request_id"] + 1,
        mpv_ingest_verified=False, external_audio_cache_known=False)


def assert_unknown(test, events):
    result = classify(events)
    test.assertEqual(result["attribution"], INCONCLUSIVE)
    test.assertEqual(result["root_cause"], UNKNOWN)
    test.assertFalse(result["audible_output_verified"])
    test.assertFalse(result["mpv_ingest_verified"])


class BVNegativeAndHealthyControls(unittest.TestCase):
    """Controls must remain fail-closed both before and after v3 exists."""

    def test_bv01_healthy_audio_video_are_independent(self):
        with site() as (fx, base):
            a, v = get(base, "audio", 0), get(base, "video", 0)
            self.assertEqual((a[0], v[0]), (200, 200))
            self.assertEqual((a[1], v[1]), (AUDIO, VIDEO))
            s = selected()
            assert_unknown(self, [s, http_event(s, fx.records[0], status=200,
                                                body_bytes=len(a[1]), complete=True)])

    def test_bv02_video_only_503_is_not_audio_failure(self):
        with site() as (fx, base):
            fx.set_fault("video", 1, "status:503")
            self.assertEqual(get(base, "video", 1)[0], 503)
            self.assertEqual(get(base, "audio", 1)[0], 200)
            s = selected()
            assert_unknown(self, [s, http_event(s, fx.records[0], status=503)])

    def test_bv03_main_demuxer_cache_does_not_prove_external_audio(self):
        s = selected()
        assert_unknown(self, [s, {"event_type":"URL_AUDIO_STREAM_CACHE",
                "source_scope":"main_demuxer", "audio_cache_duration_known":1,
                "audio_cache_duration_s":70, "source_reachability":"unverified"}])

    def test_bv04_human_mpv_log_cannot_attest_request_role(self):
        assert_unknown(self, [selected(), {"event_type":"MPV_LOG_MESSAGE",
            "message":"audio stream HTTP 503, please retry"}])

    def test_bv05_stale_epoch_does_not_rebind_failure(self):
        current = selected(epoch=2)
        prior = selected(epoch=1)
        assert_unknown(self, [current, http_event(prior,
            {"role":"audio", "request_id":1}, status=503)])

    def test_bv06_same_load_different_session_cannot_borrow(self):
        current = selected(session=4)
        prior = selected(session=3)
        assert_unknown(self, [current, http_event(prior,
            {"role":"audio", "request_id":1}, status=503)])

    def test_bv07_stop_fence_blocks_late_callback(self):
        s = selected()
        stop = {**s, "event_type":"STOP", "owner_fence_seq":2}
        assert_unknown(self, [s, stop, http_event(s,
            {"role":"audio", "request_id":1}, status=503)])

    def test_bv08_conflicting_duplicate_request_is_inconclusive(self):
        s = selected()
        first = http_event(s, {"role":"audio", "request_id":1}, status=503)
        assert_unknown(self, [s, first, {**first,"status":200,"complete":True}])

    def test_bv09_lost_event_fails_closed(self):
        s = selected()
        assert_unknown(self, [s, {"event_type":"EVENT_LOSS",
            "emitter_id":"fixture-http-owner", "lost":1},
            http_event(s, {"role":"audio", "request_id":3}, status=503)])

    def test_bv10_zero_content_does_not_mean_missing_pcm(self):
        with site() as (fx, base):
            fx.set_fault("audio", 9, "zero_pcm")
            res = get(base, "audio", 9)
            self.assertEqual((res[0], len(res[1]), set(res[1])),
                             (200, len(AUDIO), {0}))
            s = selected()
            assert_unknown(self, [s, http_event(s, fx.records[-1],
                status=200, body_bytes=len(res[1]), complete=True)])

    def test_bv11_signed_url_canary_never_leaks_via_verifier(self):
        s = selected()
        output = classify([s, {"event_type":"MPV_LOG_MESSAGE",
              "message":"https://example.invalid/audio?access_token=SECRET_CANARY"}])
        self.assertEqual(output["root_cause"], UNKNOWN)
        self.assertNotIn("SECRET_CANARY", json.dumps(output))
        self.assertNotIn("access_token=", json.dumps(output))


class BVPositiveSemanticRED(unittest.TestCase):
    """Positive source attribution requires the missing behavioral v3 contract."""

    def failure(self, kind="status:503", phase="IN_PLAY"):
        with site() as (fx, base):
            self.assertEqual(get(base, "audio", 0)[:2], (200, AUDIO))
            self.assertEqual(get(base, "video", 0)[:2], (200, VIDEO))
            fx.set_fault("audio", 1, kind)
            audio = get(base, "audio", 1)
            self.assertEqual(get(base, "video", 1)[:2], (200, VIDEO))
            observed = next(x for x in fx.records if x["role"]=="audio"
                            and x["segment"]==1)
            s = selected(phase=phase)
            event = http_event(s, observed, status=audio[0],
                               body_bytes=len(audio[1]),
                               complete=audio[0]==200 and audio[2] is None)
            return s, event, audio

    def test_bv12_owned_audio_503_requires_track_request_attribution(self):
        s, r, result = self.failure()
        self.assertEqual(result[0], 503)
        out = classify([s, r])
        self.assertEqual(out["attribution"], "AUDIO_HTTP_ERROR_OBSERVED")
        self.assertEqual(out["root_cause"], UNKNOWN)
        self.assertFalse(out["audible_output_verified"])
        self.assertFalse(out["mpv_ingest_verified"])

    def test_bv13_truncated_http_200_is_not_complete_audio_delivery(self):
        s, r, result = self.failure("truncate")
        self.assertEqual(result[2], "IncompleteRead")
        out = classify([s, r])
        self.assertEqual(out["attribution"], "AUDIO_HTTP_INCOMPLETE_OBSERVED")
        self.assertEqual(out["root_cause"], UNKNOWN)

    def test_bv14_cold_source_failure_not_midplayback(self):
        s, r, result = self.failure(phase="COLD_BEFORE_FIRST_PCM")
        self.assertEqual(result[0], 503)
        out = classify([s, r])
        self.assertEqual(out["attribution"], "COLD_AUDIO_HTTP_ERROR_OBSERVED")
        self.assertEqual(out["root_cause"], UNKNOWN)

    def test_bv15_http_error_plus_pcm_gap_cannot_prove_causality(self):
        s, r, result = self.failure()
        self.assertEqual(result[0], 503)
        gap = {**s, "event_type":"PCM_GAP_OBSERVED",
               "generation":1, "block_sequence":18}
        out = classify([s, r, gap])
        self.assertEqual(out["attribution"], "AUDIO_HTTP_ERROR_OBSERVED")
        self.assertEqual(out["root_cause"], UNKNOWN)
        self.assertEqual(out["first_broken_boundary"], UNKNOWN)
        self.assertFalse(out["audible_output_verified"])


if __name__ == "__main__":
    unittest.main()
