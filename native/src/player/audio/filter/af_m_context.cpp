#include "af_m.h"
#include "Audio.h"
#include "player/PlayerStateSystem.h"

AudioContext AudioFilterManager::ExtractCurrentContext() {
    AudioContext ctx;

    if (m_stateSystem) {
        // 1. ĐỌC THÔNG SỐ CƠ BẢN VÀ THẺ METADATA TỪ STATE SYSTEM
        m_stateSystem->ReadAudio([&](auto const& m){
            ctx.volume = static_cast<double>(m.volume.volume);
            ctx.sample_rate = m.params.asamplerate;
            ctx.channel_count = m.params.channel_count;
            ctx.bitrate_kbps = static_cast<double>(m.codec.abitrate / 1000);
            ctx.codec = m.codec.acodec;
        });

        m_stateSystem->ReadPlayback([&](PlaybackModel const& m){
            ctx.speed = static_cast<double>(m.config.speed);
        });

        m_stateSystem->ReadVideo([&](auto const& m){
            ctx.is_audio_only = (m.dimensions.width == 0 && m.dimensions.height == 0);
        });

        // 2. GHI ĐÈ BẰNG DỮ LIỆU TÍN HIỆU THỰC TẾ (REAL-TIME PCM)
        if (m_audio) {
            AudioVisualizerFrame a_ctx;
            if (m_audio->GetVisualizerData(a_ctx)) {
                // ITU-R BS.1770 Loudness Metrics
                ctx.loudness_momentary = static_cast<double>(a_ctx.momentaryLUFS);
                ctx.loudness_shortterm = static_cast<double>(a_ctx.shortTermLUFS);
                ctx.loudness_integrated = static_cast<double>(a_ctx.integratedLUFS);
                ctx.loudness_range = static_cast<double>(a_ctx.loudnessRange);
                ctx.loudness_lra_low = static_cast<double>(a_ctx.lralow);
                ctx.loudness_lra_high = static_cast<double>(a_ctx.lrahigh);
                
                // Peak & Signal Safety Metrics
                double maxPeak = static_cast<double>(std::max(a_ctx.peakLeft, a_ctx.peakRight));
                ctx.sample_peak = maxPeak;
                ctx.sample_peak_ch0 = static_cast<double>(a_ctx.peakLeft);
                ctx.sample_peak_ch1 = static_cast<double>(a_ctx.peakRight);
                
                // True Peak ước tính từ DSP
                ctx.true_peak = static_cast<double>(a_ctx.truePeakEst);
                ctx.true_peak_ch0 = static_cast<double>(a_ctx.peakLeft); 
                ctx.true_peak_ch1 = static_cast<double>(a_ctx.peakRight);

                // Các tham số Realtime mới phục vụ Feedback Sanitizer Gate
                ctx.phase_correlation = static_cast<double>(a_ctx.phaseCorrelation);
                ctx.clip_count = static_cast<int>(a_ctx.clipCount);
                ctx.sub_bass_energy = static_cast<double>(a_ctx.subBassEnergy);
                ctx.bass_energy = static_cast<double>(a_ctx.bassEnergy);
                ctx.mid_energy = static_cast<double>(a_ctx.midEnergy);
                ctx.treble_energy = static_cast<double>(a_ctx.trebleEnergy);
            }
        }

    } else if (mpv) {
        // FALLBACK: Đọc trực tiếp từ mpv_handle nếu chưa gán PlayerStateSystem
        double vol = 100.0, spd = 1.0;
        mpv_get_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
        mpv_get_property(mpv, "speed", MPV_FORMAT_DOUBLE, &spd);
        ctx.volume = vol;
        ctx.speed = spd;
    }

    return ctx;
}