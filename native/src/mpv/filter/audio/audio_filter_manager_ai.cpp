#include "audio_filter_manager.h"

AudioContext AudioFilterManager::ExtractCurrentContext() {
    AudioContext ctx;
    ctx.volume = (double)g_playbackStatus.volume;
    ctx.speed = g_playbackStatus.speed;
    ctx.sample_rate = (int64_t)g_videoInfo.g_audioarams.asamplerate;
    ctx.channel_count = (int64_t)g_videoInfo.g_audioarams.channel_count;
    ctx.bitrate_kbps = (double)(g_videoInfo.abitrate / 1000);
    ctx.codec = g_videoInfo.acodec;
    ctx.is_audio_only = g_videoInfo.width == 0 && g_videoInfo.height == 0;
    ctx.loudness_momentary = g_videoInfo.g_audioarams.loudness_momentary;
    ctx.loudness_shortterm = g_videoInfo.g_audioarams.loudness_shortterm;
    ctx.loudness_integrated = g_videoInfo.g_audioarams.loudness_integrated;
    ctx.loudness_range = g_videoInfo.g_audioarams.loudness_range;
    ctx.loudness_lra_low = g_videoInfo.g_audioarams.loudness_lra_low;
    ctx.loudness_lra_high = g_videoInfo.g_audioarams.loudness_lra_high;
    ctx.true_peak = g_videoInfo.g_audioarams.true_peak;
    ctx.true_peak_ch0 = g_videoInfo.g_audioarams.true_peak_ch0;
    ctx.true_peak_ch1 = g_videoInfo.g_audioarams.true_peak_ch1;
    ctx.sample_peak = g_videoInfo.g_audioarams.sample_peak;
    ctx.sample_peak_ch0 = g_videoInfo.g_audioarams.sample_peak_ch0;
    ctx.sample_peak_ch1 = g_videoInfo.g_audioarams.sample_peak_ch1;
    return ctx;
}

bool AudioFilterManager::CheckEnvironmentHysteresis(const AudioContext& ctx) {
    // SỬA LỖI: Chuyển sang dùng biến thành viên Class (m_lastX) để đảm bảo an toàn đa luồng
    bool changed = (std::abs(ctx.volume - m_lastVolume) > 1.5) || 
                   (std::abs(ctx.bitrate_kbps - m_lastBitrate) > 10.0) || 
                   (std::abs(ctx.speed - m_lastSpeed) > 0.05) || 
                   (ctx.channel_count != m_lastChannels) || 
                   (m_currentPreset != m_lastPreset);

    if (changed) {
        m_lastVolume = ctx.volume; 
        m_lastBitrate = ctx.bitrate_kbps;
        m_lastSpeed = ctx.speed; 
        m_lastChannels = ctx.channel_count;
        m_lastPreset = m_currentPreset;
    }
    return changed;
}

AdaptiveTargets AudioFilterManager::AnalyzeContextAndCalculateTargets(const AudioContext& ctx) 
{   
    AdaptiveTargets targets;
    
    // =========================================================================
    // 1. KHỞI TẠO CẤU HÌNH PRESET CƠ BẢN (EQUALIZER GAINS)
    // =========================================================================
    switch (m_currentPreset) {
        case AudioPreset::Pop:          targets.eq_gains = { -2.0f, -1.0f, 0.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f }; break;
        case AudioPreset::Rock:         targets.eq_gains = { 5.0f, 6.0f, 5.0f, 3.0f, -1.0f, -3.0f, -2.0f, 0.0f, 2.0f, 4.0f, 5.0f, 4.0f }; break;
        case AudioPreset::EDM_Dance:    targets.eq_gains = { 6.0f, 8.0f, 7.0f, 4.0f, 1.0f, -1.0f, 0.0f, 2.0f, 4.0f, 5.0f, 6.0f, 5.0f }; break;
        case AudioPreset::Classical:    targets.eq_gains = { -3.0f, -2.0f, -1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f }; break;
        case AudioPreset::Acoustic:     targets.eq_gains = { 2.0f, 2.0f, 3.0f, 1.0f, 0.0f, 2.0f, 3.0f, 4.0f, 4.0f, 3.0f, 3.0f, 2.0f }; break;
        case AudioPreset::Gaming_FPS:   targets.eq_gains = { -8.0f, -6.0f, -4.0f, -2.0f, 1.0f, 3.0f, 4.0f, 7.0f, 6.0f, 5.0f, 2.0f, 0.0f }; break;
        case AudioPreset::Movie_Cinema: targets.eq_gains = { 7.0f, 6.0f, 4.0f, 1.0f, -1.0f, 2.0f, 4.0f, 5.0f, 3.0f, 2.0f, 3.0f, 4.0f }; break;
        case AudioPreset::Deep_Bass:    targets.eq_gains = { 8.0f, 8.5f, 7.5f, 2.0f, -1.0f, -2.0f, -1.5f, 0.0f, 0.5f, 1.0f, 1.5f, 1.0f }; break;
        case AudioPreset::Flat:
        default:                        targets.eq_gains = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; break;
    }

    // =========================================================================
    // 2. MẠCH LỌC THỜI GIAN THỰC EMA (METRICS SMOOTHING)
    // =========================================================================
    const double alpha_fast = 0.25;
    const double alpha_slow = 0.10;
    
    if (m_smoothedTruePeak <= 0.0)   m_smoothedTruePeak = ctx.true_peak;
    if (m_smoothedSamplePeak <= 0.0) m_smoothedSamplePeak = ctx.sample_peak; 
    if (m_smoothedShortTerm >= 0.0)  m_smoothedShortTerm = ctx.loudness_shortterm;

    m_smoothedTruePeak   = (alpha_fast * ctx.true_peak) + ((1.0 - alpha_fast) * m_smoothedTruePeak);
    m_smoothedSamplePeak = (alpha_fast * ctx.sample_peak) + ((1.0 - alpha_fast) * m_smoothedSamplePeak);
    m_smoothedShortTerm  = (alpha_slow * ctx.loudness_shortterm) + ((1.0 - alpha_slow) * m_smoothedShortTerm);

    // [TỐI ƯU KHÓA 1]: Xác định shock cơ học dựa hoàn toàn vào sự thay đổi đột ngột của Momentary Loudness
    bool is_transient_shock = (ctx.loudness_momentary - m_smoothedShortTerm) > 8.0; 

    // [TỐI ƯU KHÓA 2]: CHỈ sử dụng Sample Peak thực tế để bắt Clipping kỹ thuật số.
    // Loại bỏ hoàn toàn các biến true_peak_chX khỏi đây để không bị thuật toán codec lừa.
    bool channel_clipping   = (ctx.sample_peak_ch0 > 0.98 || ctx.sample_peak_ch1 > 0.98 || ctx.sample_peak > 0.98);

    // =========================================================================
    // 3. LOGIC PHÂN TÍCH MÔI TRƯỜNG & TÍCH HỢP ĐẶC TÍNH FILE
    // =========================================================================
    float recommended_vocal_boost = 0.0f;

    if (ctx.loudness_range > 12.0) {
        targets.comp_enabled = true; targets.comp_th = -24.0f; targets.comp_rt = 3.0f;   
        recommended_vocal_boost += 2.0f;
    } 
    else if (ctx.loudness_range > 0.1 && ctx.loudness_range < 4.0 && !targets.comp_enabled) {
        targets.comp_th = -10.0f; targets.comp_rt = 1.5f;
    }

    if (ctx.loudness_integrated > -13.0) {
        float penalty = (m_currentPreset == AudioPreset::Deep_Bass) ? 0.90f : 0.70f;
        for (float& gain : targets.eq_gains) { if (gain > 0.0f) gain *= penalty; }
    }

    if (ctx.codec == "mp3" || ctx.codec == "aac" || (ctx.bitrate_kbps > 0.0 && ctx.bitrate_kbps < 192.0)) {
        targets.eq_gains[0] += 2.5f; 
        targets.eq_gains[9] += 3.5f;
        targets.crystalizer_enabled = true; 
        targets.crystalizer_i = 3.8f;

        // [ỨNG DỤNG TRUE PEAK 2]: Bộ phanh chống chói tai (Anti-Harshness)
        // Nếu thuật toán nội suy đang quá tải, việc ép thêm dải cao sẽ gây thảm họa xé tiếng
        if (m_smoothedTruePeak > 1.03) {
            targets.eq_gains[9] -= 1.5f;       // Hạ bớt dải Treble cao
            targets.crystalizer_i *= 0.6f;     // Giảm 40% cường độ Crystalizer để chất âm mượt trở lại
        }
    }

    if (!ctx.is_audio_only) {
        targets.eq_gains[4] += 2.0f; targets.eq_gains[5] += 3.0f; 
        if (ctx.channel_count == 2) { targets.stereo_enabled = true; targets.stereo_m = 3.5f; }
    }

    if (ctx.volume < 30.0) {
        targets.eq_gains[0] += 4.0f; targets.eq_gains[9] += 3.0f; 
        targets.comp_enabled = true; targets.comp_th = -22.0f; targets.comp_rt = 1.8f;
    } else if (ctx.volume > 85.0) {
        float vol_scale = (m_currentPreset == AudioPreset::Deep_Bass) ? 0.75f : 0.40f;
        for (float& gain : targets.eq_gains) { if (gain > 1.0f) gain *= vol_scale; }
        targets.comp_enabled = true; targets.comp_th = -5.0f; targets.comp_rt = 4.0f;
    }

    if (ctx.loudness_lra_low < -36.0 && ctx.loudness_lra_low > -99.0) recommended_vocal_boost += 1.5f;
    if (recommended_vocal_boost > 0.0f) {
        targets.eq_gains[5] += std::min(2.0f, recommended_vocal_boost * 0.8f);
        targets.eq_gains[6] += recommended_vocal_boost; 
        targets.eq_gains[7] += std::min(2.0f, recommended_vocal_boost * 0.8f);
    }

    // =========================================================================
    // 4. MẠCH ĐIỀU PHỐI STABILIZER KHÔNG LỒNG NHAU (DECOUPLED PIPELINES)
    // =========================================================================
    if (m_enableOuterStabilizer) {
        auto* out_comp = FindFilter("f_out_compressor");
        auto* out_lim  = FindFilter("f_out_limiter");

        if (out_comp && out_lim) {
            float curr_comp_th = out_comp->params["threshold"].current;
            float curr_comp_mk = out_comp->params["makeup"].current;
            float curr_lim_th  = out_lim->params["threshold"].current;

            // --- PHÂN LỚP A: ĐỊNH HÌNH GIÁ TRỊ ĐÍCH THEO THỂ LOẠI (PRESET BASELINE) ---
            float base_comp_th   = -12.0f; 
            float base_max_makeup = 2.0f;
            float step_modifier   = 1.0f;   

            switch (m_currentPreset) {
                case AudioPreset::Classical:
                case AudioPreset::Acoustic:
                    base_comp_th = -6.0f; base_max_makeup = 1.0f; step_modifier = 0.5f; break;
                case AudioPreset::EDM_Dance:
                case AudioPreset::Rock:
                    base_comp_th = -16.0f; base_max_makeup = 1.5f; step_modifier = 1.2f; break;
                case AudioPreset::Movie_Cinema:
                case AudioPreset::Gaming_FPS:
                    base_comp_th = -20.0f; base_max_makeup = 3.5f; step_modifier = 1.5f; break;
                case AudioPreset::Deep_Bass:
                    base_comp_th = -6.0f; base_max_makeup = 1.0f; step_modifier = 0.5f; break;
                default:
                    break;
            }

            // --- PHÂN LỚP B: ĐIỀU KHIỂN HỒI PHỤC TỰ NHIÊN (BASE EVOLUTION) ---
            // Xu hướng trả Limiter về trạng thái mở dải động tuyến tính
            // [ỨNG DỤNG TRUE PEAK 1]: Dự đoán méo Analog để siết trần bảo vệ (Ceiling Margin) một cách chậm rãi
            float safe_lim_ceiling = -0.2f; 
            if (m_smoothedTruePeak > 1.05) {
                safe_lim_ceiling = -1.5f; // File nổ Inter-sample quá nặng, ép trần xuống sâu để cứu chip DAC
            } else if (m_smoothedTruePeak > 1.00) {
                safe_lim_ceiling = -0.8f; // Có hiện tượng vượt ngưỡng analog, hạ trần vừa phải
            }

            // Thay vì ép chết cứng vào -0.2f, ta hướng Limiter hồi phục về safe_lim_ceiling
            targets.out_lim_threshold = std::min(safe_lim_ceiling, curr_lim_th + 0.12f);

            // Sử dụng m_smoothedShortTerm kiểm soát vĩ mô toàn cục
            if (m_smoothedShortTerm < -28.0) {
                // Nhạc có nền âm lượng nhỏ: Bù đắp tinh tế và nâng nhẹ Compressor
                float soft_target_th = std::max(base_comp_th, -12.0f); 
                float th_dist = std::abs(curr_comp_th - soft_target_th);
                targets.out_comp_threshold = std::min(soft_target_th, curr_comp_th + (0.06f * th_dist));
                targets.out_comp_makeup    = std::min(base_max_makeup, curr_comp_mk + 0.04f);
            } 
            else {
                // Nhạc vốn đã to (EDM/Pop): Đưa máy nén về trạng thái nghỉ ngơi thả lỏng (Transparent Leveling)
                float th_dist = std::abs(curr_comp_th - (-4.0f)); 
                targets.out_comp_threshold = std::min(-4.0f, curr_comp_th + (0.10f * th_dist * step_modifier));
                targets.out_comp_makeup    = std::max(1.0f,   curr_comp_mk - 0.05f);
            }

            // --- PHÂN LỚP C: CHUYÊN BIỆT GIẢM THIỂU SỐC ĐỘNG HỌC (TRANSIENT OVERRIDES) ---
            if (is_transient_shock) {
                targets.out_comp_threshold = std::max(-12.0f, targets.out_comp_threshold - 0.2f); 
                targets.out_comp_makeup    = std::max(1.0f,   targets.out_comp_makeup - 0.04f);
            }

            // --- PHÂN LỚP D: LÁ CHẮN BẢO VỆ CHỐNG OVERLOAD KÉP (CLIPPING PROTECTION) ---
            // [TỐI ƯU KHÓA 3]: Chỉ kích hoạt mạch bóp nghẹt khẩn cấp này khi SAMPLE PEAK thực sự chạm trần nguy hiểm.
            // Loại bỏ hoàn toàn m_smoothedTruePeak > 0.96 để nhạc hiện đại được "thở" tự nhiên ở các phân lớp trên.
            if (channel_clipping || m_smoothedSamplePeak > 0.96) {
                float highest_peak = std::max({(float)m_smoothedSamplePeak, (float)ctx.sample_peak});
                float cl_severity  = std::min(1.5f, highest_peak / 0.96f); 

                float cl_distance = std::abs(curr_lim_th - (-6.0f));
                targets.out_lim_threshold  = std::max(-6.0f, curr_lim_th - (0.20f * cl_distance * cl_severity * step_modifier));
                targets.out_comp_threshold = std::max(-15.0f, targets.out_comp_threshold - (0.5f * cl_severity));
                targets.out_comp_makeup    = std::max(1.0f,   targets.out_comp_makeup - (0.15f * cl_severity));
            }

            // --- PHÂN LỚP E: BỘ LỌC CHỐNG PUMPING VÙNG CAO TRÀO (ANTI-PUMPING ENFORCEMENT) ---
            if (m_smoothedShortTerm >= (ctx.loudness_lra_high - 2.0)) {
                float safe_floor = (m_currentPreset == AudioPreset::Classical || m_currentPreset == AudioPreset::Acoustic) ? -5.0f : -8.0f;
                targets.out_comp_threshold = std::max(targets.out_comp_threshold, safe_floor);
                if (targets.out_comp_makeup > 1.0f) {
                    targets.out_comp_makeup *= 0.97f; 
                }
            }
        }
    }

    // =========================================================================
    // 5. QUẢN LÝ BỘ KÍCH ÂM BOOSTER (MẠCH HỒI PHỤC TUYẾN TÍNH AN TOÀN)
    // =========================================================================
    if (m_enableOuterBooster) {
        auto* vol_boost = FindFilter("f_vol_booster");
        if (vol_boost && vol_boost->enabled) {
            float active_boost = vol_boost->params["volume"].current;
            float user_target  = vol_boost->params["volume"].user_target;
            
            // TÌNH HUỐNG 1: Xả Boost khẩn cấp khi mẫu số (Sample Peak) chạm trần nguy hiểm (Attack nhanh)
            if (channel_clipping || m_smoothedSamplePeak > 0.98 || ctx.sample_peak > 0.99) {
                targets.booster_volume = std::max(1.0f, active_boost - 0.08f); 
            } 
            // TÌNH HUỐNG 2: Trạng thái an toàn, tự động đưa Booster về lại user_target (Release chậm mượt)
            else {
                if (active_boost < user_target) {
                    // Tăng từ từ (+0.002f thay vì 0.01f) để người nghe không nhận ra âm lượng đang tăng (Chống Pumping)
                    targets.booster_volume = std::min(user_target, active_boost + 0.002f);
                } 
                else if (active_boost > user_target) {
                    // Nếu vì lý do gì đó lớn hơn target, hạ mượt về target
                    targets.booster_volume = std::max(user_target, active_boost - 0.005f);
                } 
                else {
                    // Đã bằng nhau thì duy trì ổn định
                    targets.booster_volume = user_target;
                }
            }
        }
    }

    return targets;
}

void AudioFilterManager::SetCurrentPreset(AudioPreset preset) {
    if (m_currentPreset != preset) {
        m_currentPreset = preset;
        if (m_autoMode) {
            UpdateAdaptiveFilters(); 
        }
    }
}

void AudioFilterManager::SetAdaptiveMode(bool enabled, AudioPreset preset) {
    if (m_globalBypass) return;
    m_autoMode = enabled;
    m_currentPreset = preset;
    
    if (enabled) {
        // Khi bật AI, ép buộc Master Volume Node phải bật để hệ thống bảo vệ hoạt động
        if (auto* vol_node = FindFilter("f_volume")) {
            vol_node->enabled = true;
        }
        UpdateAdaptiveFilters();
    } else {
        // TẮT AI: Trả các bộ lọc do AI quản lý về Flat, khôi phục các tham số về user_target gốc
        for (auto& filter : m_filters) {

            if (!filter.isBypassManagement) {
                for (auto& [key, p] : filter.params) {
                    p.current = p.user_target; 
                    p.lastSent = 999999.0f;
                }

                filter.enabled = false;
            }
        }
        
        
        EvaluateSystemSafety();
        SyncAll();
    }
}

void AudioFilterManager::UpdateAdaptiveFilters() {

    AudioContext newCtx = ExtractCurrentContext();
    bool macroEnvChanged = CheckEnvironmentHysteresis(newCtx);
    m_currentContext = newCtx;

    // 1. Giới hạn tần suất xử lý chu kỳ real-time (Throttling) để bảo vệ IPC pipe của MPV
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_lastUpdateTime).count();
    if (!macroEnvChanged && elapsed < 150) return;

    m_lastUpdateTime = currentTime;

    if (!m_autoMode || !mpv || m_globalBypass) return;

    // Lấy mục tiêu đã được lọc trung bình động (EMA) từ tầng phân tích
    AdaptiveTargets targets = AnalyzeContextAndCalculateTargets(m_currentContext);
 
    bool need_sync_structure = false;
    bool parameter_changed = false; // SỬA LỖI: Khởi tạo false để tối ưu hóa băng thông gửi lệnh

    const float epsilon = 0.02f; // Thu hẹp sai số để tiệm cận độ chính xác cao hơn

    // =========================================================================
    // TRỢ THỦ 1: HOÃN BẬT/TẮT STRUCTURAL (SOFT BYPASS / DE-CLICKING)
    // =========================================================================
    // Thay vì tắt filter ngay lập tức, ta giữ nó bật cho đến khi tham số cốt lõi lerp về 0
    auto syncFilterStateAdaptive = [&](AudioFilter* filter, bool target_state, float current_core_val, float neutral_val) -> AudioFilter* {
        if (!filter || filter->isBypassManagement) return nullptr;

        // Nếu muốn TẮT, nhưng tham số chưa kịp lùi về trạng thái trung tính (neutral) -> Hoãn tắt để tránh tiếng Pop
        if (!target_state && filter->enabled) {
            if (std::abs(current_core_val - neutral_val) > 0.1f) {
                return filter; // Tiếp tục giữ filter bật để Lerp hạ cánh an toàn
            }
        }

        // Thực thi bật/tắt khi đã thực sự an toàn
        if (filter->enabled != target_state) {
            filter->enabled = target_state;
            need_sync_structure = true;
        }
        return filter;
    };

    // =========================================================================
    // TRỢ THỦ 2 & 3: LERP THÍCH ỨNG THEO HƯỚNG CHUYỂN ĐỘNG (ATTACK VS RELEASE)
    // =========================================================================
    // Âm thanh bị gắt khi "nhả" (Release) quá nhanh. Ta cần Attack nhanh để bảo vệ, Release chậm để mượt tiếng.
    auto lerpParamBallistics = [&](AudioFilter* filter, const std::string& param_key, float target_val, 
                                   float base_alpha, bool is_external, float custom_eps = 0.02f) {
        if (!filter || filter->isBypassManagement || !filter->enabled || !filter->params.count(param_key)) return;
        
        auto& p = filter->params[param_key];
        float& current = p.current;
        float reference_val = is_external ? ((p.lastSent == 999999.0f) ? p.user_target : p.lastSent) : current;

        if (std::abs(reference_val - target_val) > custom_eps) {
            float final_alpha = base_alpha;

            // Xử lý động học cho bộ nén/giới hạn âm lượng (Threshold / Volume)
            if (param_key == "threshold" || param_key == "volume") {
                if (target_val < reference_val) {
                    // Chiều xu hướng ÉP XUỐNG (Attack Khẩn cấp) -> Cần nhanh để chống vỡ tiếng
                    final_alpha = std::min(0.40f, base_alpha * 1.5f); 
                } else {
                    // Chiều xu hướng NHẢ RA (Release Hồi phục) -> Phải thật chậm để tránh nghẹt/bơm tiếng
                    final_alpha = std::max(0.08f, base_alpha * 0.5f); 
                }
            }
            // Xử lý động học cho Gain bù (Makeup Gain) hoặc Stereo
            else if (param_key == "makeup" || param_key == "m" || param_key == "g") {
                if (target_val > reference_val) {
                    final_alpha = base_alpha * 0.6f; // Đẩy âm lượng nền lên từ từ, không giật mình
                } else {
                    final_alpha = base_alpha * 1.2f; // Hạ xuống nhanh hơn để tránh clip trần số
                }
            }

            float next_val = reference_val + final_alpha * (target_val - reference_val);
            
            current = next_val;
            if (is_external) p.lastSent = next_val;
            parameter_changed = true;
        } else {
            current = target_val;
            if (is_external && p.lastSent != target_val) {
                p.lastSent = target_val;
                parameter_changed = true;
            }
        }
    };

    // =========================================================================
    // THỰC THI CHUỖI XỬ LÝ THEO CHIẾN LƯỢC MƯỢT HÓA TOÀN DIỆN
    // =========================================================================

    // 1. Cập nhật 12 dải tần EQ Graphic (Mềm hóa bằng cơ chế hoãn ngắt cấu trúc)
    for (int i = 0; i < m_eqBands.size(); ++i) {
        const std::string& band_id = m_eqBands[i].first;
        // Lấy Filter dựa trên mảng ID dùng chung
        auto* filter = FindFilter(band_id);
        float current_gain = filter ? filter->params["g"].current : 0.0f;

        float target_gain = (i < targets.eq_gains.size()) ? targets.eq_gains[i] : 0.0f;
        // Luôn giữ EQ bật, hoặc chỉ tắt khi Gain đã lùi sát về 0.0f
        if (auto* active_filter = syncFilterStateAdaptive(filter, true, current_gain, 0.0f)) {
            lerpParamBallistics(active_filter, "g", target_gain, 0.22f, false);
        }
    }

    // 2. Cập nhật các Filter chức năng không gian nội bộ
    auto* f_cry = FindFilter("f_crystalizer");
    float curr_cry_i = f_cry ? f_cry->params["i"].current : 0.0f;
    if (auto* f = syncFilterStateAdaptive(f_cry, targets.crystalizer_enabled, curr_cry_i, 0.0f)) {
        lerpParamBallistics(f, "i", targets.crystalizer_i, 0.15f, false);
    }

    auto* f_ste = FindFilter("f_stereo");
    float curr_ste_m = f_ste ? f_ste->params["m"].current : 1.0f;
    if (auto* f = syncFilterStateAdaptive(f_ste, targets.stereo_enabled, curr_ste_m, 1.0f)) {
        lerpParamBallistics(f, "m", targets.stereo_m, 0.12f, false);
    }

    auto* f_comp = FindFilter("f_comp");
    float curr_comp_th = f_comp ? f_comp->params["threshold"].current : 0.0f;
    if (auto* f = syncFilterStateAdaptive(f_comp, targets.comp_enabled, curr_comp_th, 0.0f)) {
        lerpParamBallistics(f, "threshold", targets.comp_th, 0.20f, false);
        lerpParamBallistics(f, "ratio", targets.comp_rt, 0.20f, false);
    }

    // 3. Cập nhật Mạch bảo vệ ngoại vi (Gửi lệnh mượt xuống MPV)
    if (m_enableOuterBooster) {
        if (auto* f = FindFilter("f_vol_booster")) {
            lerpParamBallistics(f, "volume", targets.booster_volume, 0.15f, true, 0.005f);
        }
    }

    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim  = FindFilter("f_out_limiter");
    if (out_comp && out_comp->enabled && out_lim && out_lim->enabled) {
        // Áp dụng định luật Dynamic Ballistics riêng cho bộ ổn định tầng cuối chống pumping
        lerpParamBallistics(out_comp, "threshold", targets.out_comp_threshold, 0.18f, true);
        lerpParamBallistics(out_comp, "makeup", targets.out_comp_makeup, 0.12f, true);
        lerpParamBallistics(out_lim, "threshold", targets.out_lim_threshold, 0.25f, true);
    }

    // Reverb Soft-Bypass
    auto* f_rev = FindFilter("f_reverb");
    float curr_rev_mix = f_rev ? f_rev->params["mix"].current : 0.0f;
    if (auto* reverb = syncFilterStateAdaptive(f_rev, targets.reverb_enabled, curr_rev_mix, 0.0f)) {
        lerpParamBallistics(reverb, "mix", targets.reverb_enabled ? 0.3f : 0.0f, 0.10f, false);
    }

    // 4. Chỉ đẩy dữ liệu đi khi thực sự có biến động cấu trúc hoặc chỉ số thay đổi
    if (need_sync_structure || parameter_changed) {
        DispatchParametersToMPV(need_sync_structure, parameter_changed);
    }
}
void AudioFilterManager::DispatchParametersToMPV(bool need_sync_structure, bool parameter_changed) {
    if (need_sync_structure) {
        EvaluateSystemSafety();
        SyncAll(); 
    } 
    else if (parameter_changed) {
        EvaluateSystemSafety();

        std::ostringstream ss;
        for (auto& filter : m_filters) {
            if (filter.enabled && !filter.isBypassManagement) {
                for (auto& [key, p] : filter.params) {

                    // Chỉ khi nào giá trị thực tế lệch khỏi giá trị đã gửi xuống MPV trước đó một khoảng có nghĩa, 
                    // ta mới cho phép build string và bắn IPC xuống Player.
                    if (p.lastSent != 999999.0f && std::abs(p.current - p.lastSent) < 0.005f) {
                        continue; 
                    }

                    float value_to_send = p.current;

                    // VÁ LỖI ÉP LẠI KHÓA DẢI CHO COMPRESSOR KHI AI LERP ĐẨY XUỐNG
                    if (filter.name == "acompressor" && key == "threshold") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                        if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                        if (value_to_send > 1.0f) value_to_send = 1.0f;
                    }

                    if (filter.id == "f_volume" && key == "volume") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                    }

                    // Clear stream cũ, giữ nguyên buffer cấp phát trước đó
                    ss.str("");
                    ss.clear();

                    if ((filter.name == "acompressor" && (key == "threshold" || key == "makeup")) || filter.id == "f_vol_booster") {
                        ss << std::fixed << std::setprecision(5) << value_to_send;
                    } else {
                        ss << std::fixed << std::setprecision(2) << value_to_send;
                    }

                    std::string val_str = ss.str();

                    const char* cmd[] = {"af-command", filter.id.c_str(), key.c_str(), val_str.c_str(), NULL};
                    mpv_command(mpv, cmd);

                    p.lastSent = p.current;
                }
            }
        }
    }
}