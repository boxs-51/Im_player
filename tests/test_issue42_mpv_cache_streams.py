"""RED-first Issue #42 H-MPV-NET-F02: typed per-stream cache observability.

The mpv e470f8986e demuxer-cache-state ts-per-stream contains entries
with 'type' audio/video/subtitle and optional cache-duration/reader-pts/end.
This source contract prevents absent/invalid telemetry from becoming zero,
HTTP reachability, or audio dropout causality. Native unit tests follow.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
OBSERVER = ROOT / "native/src/player/event/PlaybackObserver.cpp"
PARSER = ROOT / "native/src/player/event/MpvCacheStreamTelemetry.h"


class PinnedMpvCacheNodeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.observer = OBSERVER.read_text(encoding="utf-8")
        cls.parser = PARSER.read_text(encoding="utf-8") if PARSER.exists() else ""

    def test_node_handler_consumes_cache_state_instead_of_ignoring(self):
        self.assertRegex(self.observer,
            r'else if \(strcmp\(name, "demuxer-cache-state"\) == 0\)\s*\{')
        self.assertIn("ParseMpvCacheStreams(node)", self.observer)
        self.assertNotIn('else if (strcmp(name, "demuxer-cache-state") == 0);', self.observer)

    def test_pinned_ts_per_stream_schema_uses_named_audio_video_streams(self):
        for field in ('"ts-per-stream"', '"type"', '"audio"', '"video"',
                      '"cache-duration"', '"reader-pts"', '"cache-end"'):
            self.assertIn(field, self.parser)

    def test_missing_and_malformed_numbers_are_not_coerced_to_zero(self):
        self.assertIn("durationKnown", self.parser)
        self.assertIn("readerPtsKnown", self.parser)
        self.assertIn("cacheEndKnown", self.parser)
        self.assertIn("std::isfinite", self.parser)
        self.assertIn("MPV_FORMAT_DOUBLE", self.parser)

    def test_bounded_node_traversal_rejects_oversize_and_null_child_maps(self):
        self.assertIn("MAX_CACHE_NODE_ENTRIES", self.parser)
        self.assertIn("node->u.list", self.parser)
        self.assertIn("MPV_FORMAT_NODE_ARRAY", self.parser)
        self.assertIn("MPV_FORMAT_NODE_MAP", self.parser)

    def test_observational_snapshot_scoped_main_demuxer_only(self):
        self.assertIn("stage=URL_AUDIO_STREAM_CACHE", self.observer)
        self.assertIn("source_scope=main_demuxer", self.observer)
        self.assertIn("source_reachability=unverified", self.observer)
        self.assertIn("audio_cache_duration_known=", self.observer)
        self.assertIn("video_cache_duration_known=", self.observer)

    def test_legacy_url_state_control_and_no_sensitive_url_in_cache_parser(self):
        self.assertIn("stage=URL_AUDIO_STATE", self.observer)
        self.assertIn("source_reachability=unverified", self.observer)
        self.assertNotIn("stream-open-filename", self.parser)
        self.assertNotIn("stream-path", self.parser)
        self.assertNotIn("signed_url", self.parser)


if __name__ == "__main__":
    unittest.main()
