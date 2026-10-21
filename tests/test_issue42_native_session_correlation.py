"""Issue #42 native per-block provenance contract (source-level RED-first).

This is an explicit STATIC oracle only. MSVC/CTest on Windows and subsequent
physical playback remain required to establish actual runtime correctness.
Tests are frozen before modifying any native source code.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "native" / "src" / "player"


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class NativeCorrelationPath(unittest.TestCase):
    def test_session_incarnation_minted_and_passed_by_player_session(self):
        header = source("session/PlayerSession.h")
        impl = source("session/PlayerSession.cpp")
        identity = source("audio/AudioSessionIdentity.h")
        self.assertIn("m_telemetrySessionUid", header)
        self.assertIn("NextAudioTelemetrySessionUid()", impl)
        self.assertIn("m_telemetrySessionUid", impl)
        self.assertIn("m_audio->Init(m_player->GetHandle(), m_state.get(), m_telemetrySessionUid)", impl)
        self.assertIn("std::atomic<std::uint64_t>", identity)
        self.assertIn("std::memory_order_relaxed", identity)
        self.assertIn("AudioTelemetryProcessRunUid()", identity)

    def test_init_passes_session_uid_before_capture_thread_started(self):
        audio = source("audio/Audio.cpp")
        capture = source("audio/AudioCaptureManager.cpp")
        self.assertRegex(audio, r"Audio::Init\(mpv_handle\* mpv, PlayerStateSystem\* stateSystem, std::uint64_t sessionUid\)")
        self.assertRegex(audio, r"m_audioCapture\.Init\(mpv, stateSystem, std::move\(rawProducer\), sessionUid\)")
        self.assertRegex(capture, r"AudioCaptureManager::Init\(mpv_handle\* mpv, PlayerStateSystem\* stateSystem, SpscProducer<AudioBlock> producer, std::uint64_t sessionUid\)")
        self.assertRegex(capture, r"m_telemetrySessionUid\s*=\s*sessionUid\s*;")
        self.assertLess(capture.index("m_telemetrySessionUid = sessionUid;"),
                        capture.index("StartCapture();"))

    def test_audio_block_is_stamped_with_session_run_and_original_generation(self):
        types = source("audio/AudioTypes.h")
        capture = source("audio/AudioCaptureManager.cpp")
        processor = source("audio/AudioProcessor.cpp")
        self.assertRegex(types, r"std::uint64_t telemetryRunUid\s*=\s*0\s*;")
        self.assertRegex(types, r"std::uint64_t telemetrySessionUid\s*=\s*0\s*;")
        self.assertIn("writeSlot->telemetryRunUid = AudioTelemetryProcessRunUid();", capture)
        self.assertIn("writeSlot->telemetrySessionUid = m_telemetrySessionUid;", capture)
        self.assertIn("writeSlot->startupLoadId = static_cast<std::uint64_t>(startupLoadId);", capture)
        self.assertIn("writeSlot->generation = m_activeGeneration;", capture)
        self.assertIn("*outSlot = *inSlot;", processor)

    def test_three_pcm_stages_log_same_block_identity_with_load_gen_time(self):
        stages = {
            "audio/AudioCaptureManager.cpp": ("CAPTURE_PUBLISH_RAW", "publishedRunUid", "publishedSessionUid"),
            "audio/AudioProcessor.cpp": ("PROCESSOR_ACQUIRE_RAW", "inSlot->telemetryRunUid", "inSlot->telemetrySessionUid"),
            "audio/AudioOutputWorker.cpp": ("OUTPUT_ACQUIRE_PROCESSED", "block->telemetryRunUid", "block->telemetrySessionUid"),
        }
        for path, (stage, run_source, session_source) in stages.items():
            with self.subTest(stage=stage):
                body = source(path)
                line = next(l for l in body.splitlines()
                            if f"stage={stage} load_id=%llu" in l and "STARTUP" not in l)
                for field in ("run_uid=%llu", "session_uid=%llu", "t_us=%llu", "generation=%llu"):
                    self.assertIn(field, line, (stage, field))
                self.assertIn(run_source, body)
                self.assertIn(session_source, body)

    def test_idle_and_segment_underflow_emit_original_run_session_identity(self):
        body = source("audio/AudioOutputWorker.cpp")
        self.assertIn("lastSuccessfulRunUid = block->telemetryRunUid;", body)
        self.assertIn("lastSuccessfulSessionUid = block->telemetrySessionUid;", body)
        evidence_lines = [line for line in body.splitlines()
                          if "stage=SDL_QUEUE_UNDERFLOW load_id=%llu" in line]
        self.assertEqual(len(evidence_lines), 2)
        for line in evidence_lines:
            self.assertIn("run_uid=%llu", line)
            self.assertIn("session_uid=%llu", line)
        self.assertIn("lastSuccessfulSessionUid", body)
        self.assertIn("block->telemetrySessionUid", body)


if __name__ == "__main__":
    unittest.main()
