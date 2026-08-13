#include "af_m.h"

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
        case AudioPreset::Karaoke:      targets.eq_gains = { 2.0f, 3.0f, 1.0f, -2.0f, -5.0f, -8.0f, -8.0f, -5.0f, -2.0f, 1.0f, 3.0f, 2.0f }; break;
        case AudioPreset::Audio_Restoration: targets.eq_gains = { -4.0f, -2.0f, 0.0f, 1.0f, 2.0f, 1.0f, 0.0f, -1.0f, -2.0f, -3.0f, -4.0f, -5.0f }; break;
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

    // =========================================================================
    // 6. ĐIỀU KHIỂN CÁC BỘ LỌC CHUYÊN BIỆT & THAM SỐ (LOGIC TỰ ĐỘNG)
    // =========================================================================
    // Kích hoạt/tắt bộ lọc dựa trên cờ trạng thái do người dùng quản lý
    targets.speech_enhancement_enabled = m_specializedFilterState.speech_enhancement;
    targets.noise_reduction_enabled = m_specializedFilterState.noise_reduction;
    targets.audio_restoration_enabled = m_specializedFilterState.audio_restoration;

    // Logic AI để điều chỉnh tham số cho Noise Reduction
    if (targets.noise_reduction_enabled) {
        if (m_smoothedShortTerm > -25.0) { // Phát hiện môi trường ồn ào
            targets.noise_reduction_level = std::min(1.0f, (float)((m_smoothedShortTerm + 25.0) / 15.0)); // Tăng mức giảm nhiễu
        } else {
            targets.noise_reduction_level = 0.0f; // Môi trường yên tĩnh, không cần giảm nhiễu
        }
    }

    // Logic AI để điều chỉnh tham số cho Speech Enhancement
    if (targets.speech_enhancement_enabled) {
        if (ctx.hearing_impaired) {
            targets.speech_enhancement_level = -15.0f; // Tăng cường mạnh cho người khiếm thính
        } else if (ctx.loudness_momentary < -30.0 && m_smoothedShortTerm > -40.0) { // Giọng nói có thể bị nhỏ trong nền ồn
            targets.speech_enhancement_level = std::min(-5.0f, (float)(m_smoothedShortTerm / 2.0)); // Tăng cường dựa trên tiếng ồn nền
        } else {
            targets.speech_enhancement_level = -25.0f; // Mặc định hoặc không tăng cường mạnh
        }
    }

    // Xử lý đặc biệt cho preset Karaoke (kích hoạt Vocal Remover)
    if (m_currentPreset == AudioPreset::Karaoke) {
        targets.vocal_remover_enabled = true;
    }
    if (m_currentPreset == AudioPreset::Audio_Restoration) {
        targets.audio_restoration_enabled = true;
    }

    return targets;
}