#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

checks = {
    "native/src/player/audio/AudioTelemetry.h": [
        "IM_PLAYER_STARTUP_BOUNDARY_TRACE",
        "EmitStartupBoundaryEvidence",
        "STARTUP_BOUNDARY %s",
    ],
    "native/src/player/audio/AudioTypes.h": [
        "startupLoadId",
        "captureReadMicros",
        "capturePublishMicros",
    ],
    "native/src/player/PlayerDataModels.h": [
        "pauseForCache",
    ],
    "native/src/player/event/PlaybackObserver.cpp": [
        "m.flags.pauseForCache = value",
    ],
    "native/src/player/audio/SdlAudioDevice.h": [
        "SetAutoPlaybackStart",
        "StartPlaybackIfPrebuffered",
        "IsPlaybackStarted",
        "m_autoPlaybackStart",
    ],
    "native/src/player/audio/SdlAudioDevice.cpp": [
        "m_autoPlaybackStart.store(true",
        "StartPlaybackIfPrebuffered",
    ],
    "native/src/player/audio/AudioCaptureManager.cpp": [
        "stage=CAPTURE_READ_COMPLETE",
        "stage=CAPTURE_PUBLISH_RAW",
        "stage=AUDIO_MEDIA_CLOCK_ANCHOR",
        "raw_ring_size=%zu",
    ],
    "native/src/player/audio/AudioProcessor.cpp": [
        "stage=PROCESSOR_ACQUIRE_RAW",
        "stage=PROCESSOR_ANALYZE_BEGIN",
        "stage=PROCESSOR_ANALYZE_END",
        "stage=PROCESSOR_PUBLISH_PROCESSED",
        "processed_ring_size=%zu",
    ],
    "native/src/player/audio/AudioOutputWorker.cpp": [
        "stage=OUTPUT_ACQUIRE_PROCESSED",
        "stage=SDL_SEGMENT_WRITE",
        "stage=SDL_PLAYBACK_STARTED",
        "stage=SDL_QUEUE_UNDERFLOW",
        "stage=STARTUP_CLOCK_RELEASE_GATE_PRIMED",
        "stage=STARTUP_CLOCK_RELEASE_GATE_ARM",
        "stage=STARTUP_CLOCK_RELEASE_GATE_WAIT",
        "stage=STARTUP_CLOCK_RELEASE_GATE_GRANTED",
        "STARTUP_CLOCK_RELEASE_LIVENESS_FALLBACK",
        "reason=%s",
        "\"clock_transition\"",
        "kStartupClockTransitionEpsilon = 0.000001",
        "kStartupClockGateHeadroomReserveBlocks = 4",
        "pause_for_cache=%d",
        "cache_buffering_state=%d",
        "playback_time=%.6f",
        "stage=STARTUP_SYNC_SAMPLE checkpoint_s=%llu",
        "emitCheckpoint(1, startupCheckpoint1sEmitted)",
        "emitCheckpoint(5, startupCheckpoint5sEmitted)",
        "emitCheckpoint(10, startupCheckpoint10sEmitted)",
    ],
}

errors = []
for rel, tokens in checks.items():
    text = (ROOT / rel).read_text(encoding="utf-8")
    for token in tokens:
        if token not in text:
            errors.append(f"{rel}: missing {token!r}")

audio_types = (ROOT / "native/src/player/audio/AudioTypes.h").read_text(encoding="utf-8")
if "kAudioOutputTargetQueueMilliseconds = 120" not in audio_types:
    errors.append("Issue #38 measurement slice must not change the 120ms queue target")
if "kAudioOutputDesignMaxQueueMilliseconds = 150" not in audio_types:
    errors.append("Issue #38 measurement slice must not change the 150ms queue hard cap")

output_worker = (ROOT / "native/src/player/audio/AudioOutputWorker.cpp").read_text(encoding="utf-8")
if "kStartupClockReleaseAdvanceSeconds" in output_worker:
    errors.append("Issue #38 production gate must use semantic MPV clock transition, not a fixed media-time threshold")

if errors:
    for item in errors:
        print(f"[ISSUE-38] ERROR {item}")
    raise SystemExit(1)

print("[ISSUE-38] startup boundary telemetry sanity PASS")
