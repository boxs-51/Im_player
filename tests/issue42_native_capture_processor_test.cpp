// #42 NAT02: real capture + AudioProcessor -> processed SPSC (not SDL).
#include "player/audio/AudioCaptureManager.h"
#include "player/audio/AudioProcessor.h"
#include "player/audio/AudioSessionIdentity.h"
#include "player/PlayerStateSystem.h"
#include "player/audio/SpscRingBuffer.h"
#include "player/audio/AudioTypes.h"
#include <mpv/client.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <utility>

namespace {
constexpr std::uint64_t session = 4243;
struct Data {
    std::uint64_t blocks=0, frames=0, nonzero=0, lastSequence=0, run=0;
    bool first=true, valid=true;
};
void consume(SpscConsumer<AudioBlock>& reader, Data& d) {
    while (const AudioBlock* b=reader.acquire_read()) {
        const auto count=b->sample_count();
        if (!b->frames || b->frames>kMaxAudioFrames ||
            b->format.channels!=kCanonicalAudioChannels ||
            b->format.sampleRate!=kCanonicalAudioSampleRate ||
            b->format.format!=AudioSampleFormat::Float32 ||
            count>b->samples.size() ||
            b->telemetrySessionUid!=session ||
            !b->telemetryRunUid || !b->captureReadMicros ||
            b->capturePublishMicros<b->captureReadMicros ||
            (!d.first && (b->sequence!=d.lastSequence+1 ||
                          b->telemetryRunUid!=d.run))) d.valid=false;
        if (d.valid) for (size_t i=0;i<count;++i) {
            if (!std::isfinite(b->samples[i])) { d.valid=false; break; }
            if (std::abs(b->samples[i])>0.000001f) ++d.nonzero;
        }
        d.first=false;
        d.lastSequence=b->sequence;
        d.run=b->telemetryRunUid;
        d.frames+=b->frames; ++d.blocks;
        reader.release_read();
        if (!d.valid) break;
    }
}
bool opt(mpv_handle* m, const char* n, const char* v) {
    return mpv_set_option_string(m,n,v)>=0;
}
}
int main(int argc, char** argv) {
    if (argc!=2 || !argv[1] ||
        std::strncmp(argv[1],"http://127.0.0.1:",17)!=0) return 10;
    mpv_handle* mpv=mpv_create();
    if (!mpv) return 11;
    if (!opt(mpv,"terminal","no") || !opt(mpv,"vo","null") ||
        !opt(mpv,"audio-display","no") || mpv_initialize(mpv)<0) {
        mpv_terminate_destroy(mpv); return 12;
    }
    int rc=1;
    {
        PlayerStateSystem state;
        auto [rawW,rawR]=make_spsc_ring_buffer<AudioBlock>(16);
        auto [procW,procR]=make_spsc_ring_buffer<AudioBlock>(16);
        AudioCaptureManager capture;
        AudioProcessor processor;
        bool capReady=capture.Init(mpv,&state,std::move(rawW),session);
        if (!capReady) rc=13;
        else {
            if (!processor.Init(std::move(rawR),std::move(procW))) rc=14;
            else {
                processor.Start();
                const char* cmd[]={"loadfile",argv[1],"replace",nullptr};
                if (mpv_command(mpv,cmd)<0) rc=15;
                else {
                    Data d;
                    const auto deadline=std::chrono::steady_clock::now()+
                                        std::chrono::seconds(7);
                    while (std::chrono::steady_clock::now()<deadline) {
                        consume(procR,d);
                        if (!d.valid) { rc=16; break; }
                        const auto m=capture.GetMetrics();
                        if (d.blocks>=2 && d.frames>=2400 && d.nonzero>0 &&
                            m.firstPipeConnectedMicros &&
                            m.firstPipeBytesMicros &&
                            m.firstCompletePcmBlockMicros) {
                            rc=0; break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(12));
                    }
                    const auto m=capture.GetMetrics();
                    if (rc==0 && (m.blocksReceived<d.blocks ||
                        m.bytesReceived<d.frames*kCanonicalAudioBytesPerFrame ||
                        d.run!=AudioTelemetryProcessRunUid())) rc=17;
                    if (rc==0) std::printf(
                        "I42_NAT02_CAPTURE_PROCESSOR_SPSC_PASS "
                        "processed_blocks=%llu processed_frames=%llu "
                        "nonzero_samples=%llu "
                        "capture=REAL_SOURCE processor=REAL_SOURCE "
                        "sdl=NOT_RUN audible=NOT_RUN root_cause=UNKNOWN\n",
                        static_cast<unsigned long long>(d.blocks),
                        static_cast<unsigned long long>(d.frames),
                        static_cast<unsigned long long>(d.nonzero));
                }
                capture.Shutdown();
                processor.Stop();
            }
            if (rc==14) capture.Shutdown();
        }
    }
    mpv_terminate_destroy(mpv);
    std::fflush(stdout);
    return rc;
}
