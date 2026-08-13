#include "af_m.h"
#include "player/PlayerStateSystem.h"

AudioContext AudioFilterManager::ExtractCurrentContext() {
    AudioContext ctx;

    if (m_stateSystem) {
        // ĐỌC CHUẨN TỪ AUTHORITATIVE STATE SYSTEM CỦA SESSION
        m_stateSystem->ReadAudio([&](auto const& m){
            ctx.volume = static_cast<double>(m.volume.volume);
            ctx.sample_rate = m.params.asamplerate;
            ctx.channel_count = m.params.channel_count;
            ctx.bitrate_kbps = static_cast<double>(m.codec.abitrate / 1000);
            ctx.codec = m.codec.acodec;
            ctx.loudness_momentary = m.loudness.loudness_momentary;
            ctx.loudness_shortterm = m.loudness.loudness_shortterm;
            ctx.loudness_integrated = m.loudness.loudness_integrated;
            ctx.loudness_range = m.loudness.loudness_range;
            ctx.loudness_lra_low = m.loudness.loudness_lra_low;
            ctx.loudness_lra_high = m.loudness.loudness_lra_high;
            ctx.true_peak = m.loudness.true_peak;
            ctx.true_peak_ch0 = m.loudness.true_peak_ch0;
            ctx.true_peak_ch1 = m.loudness.true_peak_ch1;
            ctx.sample_peak = m.loudness.sample_peak;
            ctx.sample_peak_ch0 = m.loudness.sample_peak_ch0;
            ctx.sample_peak_ch1 = m.loudness.sample_peak_ch1;

        });

        m_stateSystem->ReadPlayback([&](PlaybackModel const& m){
            ctx.speed = static_cast<double>(m.config.speed);
        });
        m_stateSystem->ReadVideo([&](auto const& m){
            ctx.is_audio_only = (m.dimensions.width == 0 && m.dimensions.height == 0);
        });

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