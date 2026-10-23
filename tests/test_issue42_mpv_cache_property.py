"""RED-first Issue #42: libmpv cache-paused PROPERTY, not the cache-pause OPTION.

T1 source contract only. The separately pinned native libmpv must also be
probed in the Windows CI process before classifying this as runtime proven.
No audio dataplane or user-media exercise is performed.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OBSERVER = ROOT / "native/src/player/event/PlaybackObserver.cpp"


class MpvCachePropertyNameContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = OBSERVER.read_text(encoding="utf-8")

    def test_canonical_paused_for_cache_flag_is_observed(self):
        self.assertRegex(self.src,
            r'\{"paused-for-cache",\s*MPV_FORMAT_FLAG\}')

    def test_canonical_property_change_updates_playback_model(self):
        self.assertRegex(self.src,
            r'strcmp\(name, "paused-for-cache"\)\s*==\s*0\)\s*'
            r'm_state\.WritePlayback\(\[&\]\(auto& m\)\s*'
            r'\{\s*m\.flags\.pauseForCache\s*=\s*value;')

    def test_canonical_cache_pause_change_emits_url_audio_snapshot(self):
        self.assertRegex(self.src,
            r'strcmp\(prop->name, "paused-for-cache"\)\s*==\s*0\s*\|\|')
        self.assertIn('emitUrlAudioSnapshot("network_cache_changed")', self.src)

    def test_wrong_option_name_never_used_as_observable_property(self):
        self.assertNotIn('"pause-for-cache"', self.src)

    def test_existing_cache_buffer_and_network_controls_preserved(self):
        self.assertIn('{"cache-buffering-state", MPV_FORMAT_INT64}', self.src)
        self.assertIn('{"demuxer-via-network", MPV_FORMAT_FLAG}', self.src)
        self.assertIn('emitUrlAudioSnapshot("audio_reconfig")', self.src)
        self.assertIn('source_reachability=unverified', self.src)


if __name__ == "__main__":
    unittest.main()
