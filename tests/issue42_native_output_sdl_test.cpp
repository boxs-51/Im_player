// NAT03: test-owned 127.0.0.1 WAV -> production Capture -> Processor -> OutputWorker -> SDL dummy.
// Only compiled original runtime .cpp objects. No GUI/physical audio or production patch.
#include "player/audio/AudioCaptureManager.h"
#include "player/audio/AudioProcessor.h"
#include "player/audio/AudioOutputWorker.h"
#include "player/audio/AudioSessionIdentity.h"
#include "player/PlayerStateSystem.h"
#include "player/audio/SpscRingBuffer.h"
#include "player/audio/AudioTypes.h"
#include <mpv/client.h>
#include <SDL.h>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <utility>

namespace {
constexpr std::uint64_t sessionUid = 4244;
bool option(mpv_handle* p,const char* n,const char* v) {
    return mpv_set_option_string(p,n,v)>=0;
}
void report(const char* mode,const AudioPipelineMetrics& raw,const AudioOutputMetrics& out) {
    std::printf("I42_NAT03_REAL_OUTPUT_OBSERVED mode=%s "
        "raw_blocks=%llu sdl_blocks=%llu first_sdl_write=%d "
        "queue_nonzero=%d highwater_bytes=%u queued_bytes=%u "
        "format_drops=%llu write_failures=%llu underflows=%llu "
        "driver=SDL_DUMMY capture=REAL_SOURCE processor=REAL_SOURCE "
        "output=REAL_SOURCE sdl=REAL_SOURCE "
        "audible=NOT_RUN midplay_503=NOT_RUN root_cause=UNKNOWN\n",
        mode,
        static_cast<unsigned long long>(raw.blocksReceived),
        static_cast<unsigned long long>(out.blocksWritten),
        out.firstSdlWriteMicros>0?1:0,
        out.firstNonzeroQueueMicros>0?1:0,
        out.queueHighWaterBytes,out.queuedBytes,
        static_cast<unsigned long long>(out.blocksDroppedFormatMismatch),
        static_cast<unsigned long long>(out.writeFailures),
        static_cast<unsigned long long>(out.queueUnderflowEvents));
}
}
int main(int argc,char** argv) {
    if(argc!=3 || !argv[1] || !argv[2] ||
       std::strncmp(argv[1],"http://127.0.0.1:",17)!=0 ||
       (std::strcmp(argv[2],"healthy")!=0 &&
        std::strcmp(argv[2],"pressure")!=0 &&
        std::strcmp(argv[2],"cold503")!=0)) return 10;
    const char* driver=std::getenv("SDL_AUDIODRIVER");
    if(!driver || std::strcmp(driver,"dummy")!=0) return 11;
    mpv_handle* mpv=mpv_create();
    if(!mpv) return 12;
    if(!option(mpv,"terminal","no") || !option(mpv,"vo","null") ||
       !option(mpv,"audio-display","no") || mpv_initialize(mpv)<0) {
        mpv_terminate_destroy(mpv); return 13;
    }
    int rc=1;
    {
        PlayerStateSystem state;
        auto [rawWriter,rawReader]=make_spsc_ring_buffer<AudioBlock>(32);
        auto [procWriter,procReader]=make_spsc_ring_buffer<AudioBlock>(32);
        AudioCaptureManager capture;
        AudioProcessor processor;
        AudioOutputWorker output;
        if (!capture.Init(mpv,&state,std::move(rawWriter),sessionUid)) rc=14;
        else {
            const bool processorReady=processor.Init(std::move(rawReader),std::move(procWriter));
            if(!processorReady) rc=15;
            else if(!output.Init(std::move(procReader),&state,AudioBackendType::SDL2)) rc=16;
            else if(!SDL_GetCurrentAudioDriver() ||
                    std::strcmp(SDL_GetCurrentAudioDriver(),"dummy")!=0) rc=17;
            else {
                output.Start();
                processor.Start();
                const char* cmd[]={"loadfile",argv[1],"replace",nullptr};
                if(mpv_command(mpv,cmd)<0) rc=18;
                else {
                    const auto deadline=std::chrono::steady_clock::now()+
                        std::chrono::seconds(std::strcmp(argv[2],"pressure")==0?7:6);
                    while(std::chrono::steady_clock::now()<deadline) {
                        const auto raw=capture.GetMetrics();
                        const auto out=output.GetMetrics();
                        const bool healthy=std::strcmp(argv[2],"healthy")==0;
                        if(healthy && raw.blocksReceived>=2 &&
                           out.blocksWritten>=2 && out.firstSdlWriteMicros>0 &&
                           out.firstNonzeroQueueMicros>0 &&
                           out.queueHighWaterBytes>0 &&
                           out.queueHighWaterBytes<=kAudioOutputDesignMaxQueueBytes &&
                           !out.blocksDroppedFormatMismatch && !out.writeFailures) {
                            rc=0; break;
                        }
                        if (!healthy && std::strcmp(argv[2],"pressure")==0 &&
                            raw.blocksReceived>=40 && out.blocksWritten>=1) {
                            // Continue into a sustained pressure window; do not infer gap here.
                            rc=0;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    }
                    const auto raw=capture.GetMetrics();
                    const auto out=output.GetMetrics();
                    if(std::strcmp(argv[2],"pressure")==0) {
                        if(out.blocksWritten==0 || raw.blocksReceived<40)
                            rc=19;
                        else rc=0; // Python independently checks actual processor-drop log.
                    }
                    if(std::strcmp(argv[2],"cold503")==0) {
                        // Cold 503 must never emit the positive healthy marker.
                        rc=(out.blocksWritten==0 && out.firstSdlWriteMicros==0)?20:21;
                    }
                    if(rc==0) {
                        report(argv[2],raw,out);
                    } else {
                        std::printf("I42_NAT03_SOURCE_OR_QUEUE_RED mode=%s "
                                    "exit=%d sdl_queue_verified=0 root_cause=UNKNOWN\n",
                                    argv[2],rc);
                    }
                }
                capture.Shutdown();
                processor.Stop();
                output.Stop();
            }
            if(rc==16 || rc==17) {
                capture.Shutdown();
                output.Stop();
            }
            if(rc==15) capture.Shutdown();
        }
    }
    mpv_terminate_destroy(mpv);
    std::fflush(stdout);
    return rc;
}
