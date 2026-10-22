// Issue #42 NAT01 — production AudioCaptureManager + real libmpv + real SPSC.
// This is a separate non-GUI TEST EXECUTABLE; no production audio source changes.
// Healthy localhost HTTP WAV proves capture, named-pipe receipt and raw ring.
// A 503 must never be misclassified as successful PCM capture.
// This does not execute the application's AudioProcessor/SDL or audible output.
#include "player/audio/AudioCaptureManager.h"
#include "player/audio/AudioSessionIdentity.h"
#include "player/PlayerStateSystem.h"
#include "player/audio/SpscRingBuffer.h"
#include "player/audio/AudioTypes.h"

#include <mpv/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {
constexpr std::uint64_t kSession = 4242;
constexpr std::uint32_t kExpectedRate = 48000;
constexpr std::uint64_t kMinFrames = 2400;
constexpr std::uint64_t kMinBlocks = 2;

struct Observations {
    std::uint64_t blocks = 0;
    std::uint64_t frames = 0;
    std::uint64_t nonzeroSamples = 0;
    std::uint64_t lastSequence = 0;
    std::uint64_t runUid = 0;
    bool first = true;
    bool valid = true;
};

void consume(SpscConsumer<AudioBlock>& reader, Observations& obs) {
    while (const AudioBlock* block = reader.acquire_read()) {
        if (block->frames == 0 || block->frames > kMaxAudioFrames ||
            block->format.channels != kCanonicalAudioChannels ||
            block->format.sampleRate != kExpectedRate ||
            block->format.format != AudioSampleFormat::Float32 ||
            block->telemetrySessionUid != kSession ||
            block->telemetryRunUid == 0 ||
            block->captureReadMicros == 0 ||
            block->capturePublishMicros < block->captureReadMicros ||
            (!obs.first && block->sequence != obs.lastSequence + 1) ||
            (!obs.first && block->telemetryRunUid != obs.runUid) ||
            block->sample_count() !=
                static_cast<size_t>(block->frames) * kCanonicalAudioChannels) {
            obs.valid = false;
        }
        for (size_t i = 0; i < block->sample_count(); ++i) {
            const float sample = block->samples[i];
            if (!std::isfinite(sample))
                obs.valid = false;
            else if (std::abs(sample) > 0.000001f)
                ++obs.nonzeroSamples;
        }
        obs.first = false;
        obs.runUid = block->telemetryRunUid;
        obs.lastSequence = block->sequence;
        obs.frames += block->frames;
        ++obs.blocks;
        reader.release_read();
        if (!obs.valid)
            break;
    }
}

bool set_option(mpv_handle* mpv, const char* name, const char* value) {
    return mpv_set_option_string(mpv, name, value) >= 0;
}
}

int main(int argc, char** argv) {
    if (argc != 2 || !argv[1] ||
        std::strncmp(argv[1], "http://127.0.0.1:", 17) != 0) {
        std::puts("I42_NAT01_UNSAFE_OR_MISSING_LOCALHOST_AUDIO_SOURCE");
        return 10;
    }

    mpv_handle* mpv = mpv_create();
    if (!mpv) {
        std::puts("I42_NAT01_MPV_CREATE_FAILED");
        return 11;
    }

    if (!set_option(mpv, "terminal", "no") ||
        !set_option(mpv, "vo", "null") ||
        !set_option(mpv, "audio-display", "no") ||
        mpv_initialize(mpv) < 0) {
        std::puts("I42_NAT01_MPV_INIT_FAILED");
        mpv_terminate_destroy(mpv);
        return 12;
    }

    int status = 1;
    {
        PlayerStateSystem state;
        auto [rawWriter, rawReader] =
            make_spsc_ring_buffer<AudioBlock>(8);
        AudioCaptureManager capture;
        if (!capture.Init(mpv, &state, std::move(rawWriter), kSession)) {
            std::puts("I42_NAT01_REAL_CAPTURE_INIT_FAILED");
            status = 13;
        } else {
            const char* cmd[] = {"loadfile", argv[1], "replace", nullptr};
            if (mpv_command(mpv, cmd) < 0) {
                std::puts("I42_NAT01_LOAD_COMMAND_FAILED");
                status = 14;
            } else {
                Observations obs;
                const auto end =
                    std::chrono::steady_clock::now() + std::chrono::seconds(7);
                while (std::chrono::steady_clock::now() < end) {
                    consume(rawReader, obs);
                    if (!obs.valid)
                        break;
                    const auto metrics = capture.GetMetrics();
                    if (obs.blocks >= kMinBlocks &&
                        obs.frames >= kMinFrames &&
                        obs.nonzeroSamples > 0 &&
                        metrics.firstPipeConnectedMicros > 0 &&
                        metrics.firstPipeBytesMicros > 0 &&
                        metrics.firstCompletePcmBlockMicros > 0) {
                        status = 0;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(12));
                }
                const auto metrics = capture.GetMetrics();
                if (!obs.valid) {
                    std::puts("I42_NAT01_REAL_CAPTURE_RAW_BLOCK_INVALID");
                    status = 15;
                }
                if (status == 0 &&
                    (metrics.blocksReceived < obs.blocks ||
                     metrics.bytesReceived < obs.frames * 8 ||
                     metrics.firstPipeConnectedMicros == 0 ||
                     metrics.firstPipeBytesMicros == 0 ||
                     metrics.firstCompletePcmBlockMicros == 0 ||
                     obs.runUid != AudioTelemetryProcessRunUid())) {
                    std::puts("I42_NAT01_REAL_CAPTURE_METRIC_MISMATCH");
                    status = 16;
                }
                if (status == 0) {
                    std::printf(
                        "I42_NAT01_PRODUCTION_CAPTURE_SPSC_PASS "
                        "raw_blocks=%llu raw_frames=%llu nonzero_samples=%llu "
                        "pipe_connected=true first_pipe_bytes=true "
                        "capture_manager=REAL_SOURCE linked_with_libmpv=true "
                        "processor=NOT_RUN sdl=NOT_RUN audible=NOT_RUN "
                        "original_source_root=UNKNOWN\n",
                        static_cast<unsigned long long>(obs.blocks),
                        static_cast<unsigned long long>(obs.frames),
                        static_cast<unsigned long long>(obs.nonzeroSamples));
                } else if (status == 1) {
                    std::printf(
                        "I42_NAT01_PRODUCTION_CAPTURE_RED "
                        "raw_blocks=%llu raw_frames=%llu nonzero_samples=%llu "
                        "pipe_connected=%d first_pipe_bytes=%d "
                        "capture_manager=REAL_SOURCE root_cause=UNKNOWN\n",
                        static_cast<unsigned long long>(obs.blocks),
                        static_cast<unsigned long long>(obs.frames),
                        static_cast<unsigned long long>(obs.nonzeroSamples),
                        metrics.firstPipeConnectedMicros != 0 ? 1 : 0,
                        metrics.firstPipeBytesMicros != 0 ? 1 : 0);
                }
            }
            capture.Shutdown(); // Join real pipe-reader before mpv teardown.
        }
    }
    mpv_terminate_destroy(mpv);
    std::fflush(stdout);
    return status;
}
