#include "af_m.h"
#include "af_m_log.h"

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
        if (auto* vol_node = FindFilter("f_volume")) {
            vol_node->enabled = true;
        }
        UpdateAdaptiveFilters();
    } else {
        // TẮT AI: Trả các bộ lọc về trạng thái do người dùng định nghĩa
        for (auto& filter : m_filters) {
            if (!filter.isBypassManagement) {
                for (auto& [key, p] : filter.params) {
                    p.current = p.user_target; 
                    p.lastSent = 999999.0f; // Đánh dấu cần đồng bộ lại
                }
                filter.enabled = false;
            } else {
                // ĐỐI VỚI BYPASS MANAGEMENT: Giữ nguyên trạng thái enable/disable của người dùng
                for (auto& [key, p] : filter.params) {
                    p.current = p.user_target;
                    // Không đặt lastSent về rác bừa bãi để tránh lỗi nhảy giá trị độc hại
                }
            }
        }
        
        EvaluateSystemSafety();
        SyncAll(); // Hàm này sẽ xây dựng lại toàn bộ chuỗi af string của MPV
    }
}

void AudioFilterManager::UpdateAdaptiveFilters() {
    DrainControlCommands();

    AudioContext newCtx = ExtractCurrentContext();
    bool macroEnvChanged = CheckEnvironmentHysteresis(newCtx);
    m_currentContext = newCtx;

    // 1. Giới hạn tần suất xử lý chu kỳ real-time (Throttling) để bảo vệ IPC pipe của MPV
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_lastUpdateTime).count();
    if (!macroEnvChanged && elapsed < 150) {
        PublishControlSnapshot();
        return;
    }

    m_lastUpdateTime = currentTime;

    if (!m_autoMode || !mpv || m_globalBypass) {
        PublishControlSnapshot();
        return;
    }

    // Lấy mục tiêu đã được lọc trung bình động (EMA) từ tầng phân tích
    AdaptiveTargets targets = AnalyzeContextAndCalculateTargets(m_currentContext);
 
    bool need_sync_structure = false;
    bool parameter_changed = false; // SỬA LỖI: Khởi tạo false để tối ưu hóa băng thông gửi lệnh

    const float epsilon = 0.02f; // Thu hẹp sai số để tiệm cận độ chính xác cao hơn

    for (auto& filter : m_filters) {
        if (filter.isBypassManagement) {
            for (auto& [key, p] : filter.params) {
                // Nếu giá trị hiện tại bị lệch khỏi target của người dùng do AI từng can thiệp
                if (std::abs(p.current - p.user_target) > 0.001f) {
                    p.current = p.user_target;
                    p.lastSent = 999999.0f; // Vô hiệu hóa bộ lọc throttle 0.005f để ép gửi ngay lập tức
                    parameter_changed = true;
                }
            }
        }
    }

    // =========================================================================
    // TRỢ THỦ 1: HOÃN BẬT/TẮT STRUCTURAL (SOFT BYPASS / DE-CLICKING)
    // =========================================================================
    // Thay vì tắt filter ngay lập tức, ta giữ nó bật cho đến khi tham số cốt lõi lerp về 0
    auto syncFilterStateAdaptive = [&](AudioFilter* filter, bool target_state, float current_core_val, float neutral_val) -> AudioFilter* {
        if (!filter || filter->isBypassManagement || !filter->ai_controllable) return nullptr;

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
        if (!filter || !filter->enabled || !filter->params.count(param_key)) return;
        
        auto& p = filter->params[param_key];

        // KIỂM TRA QUYỀN: AI chỉ được thay đổi tham số nếu được cho phép
        if (!p.ai_controllable) return;

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
            //if (is_external) p.lastSent = next_val;
            parameter_changed = true;
        } else {
            if (current != target_val) {
                current = target_val;
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

    // =========================================================================
    // 4. TỰ ĐỘNG ĐIỀU KHIỂN CÁC BỘ LỌC CHUYÊN BIỆT VÀ THAM SỐ CỦA CHÚNG
    // =========================================================================
    for (auto& filter : m_filters) {
        // Bỏ qua nếu filter không được AI quản lý hoặc không thuộc nhóm chuyên biệt
        if (!filter.ai_controllable || filter.group != "specialized") {
            continue;
        }

        bool target_state = false;
        float core_value_for_bypass = 0.0f; // Giá trị cốt lõi để kiểm tra soft-bypass
        float neutral_val_for_bypass = 0.0f; // Giá trị trung tính cho soft-bypass

        // Xác định trạng thái mục tiêu và giá trị cốt lõi cho soft-bypass
        if (filter.id == "f_speech_enhancement") {
            target_state = m_specializedFilterState.speech_enhancement;
            core_value_for_bypass = filter.params.count("level") ? filter.params.at("level").current : -25.0f;
            neutral_val_for_bypass = -25.0f; // Mức trung tính cho tăng cường giọng nói
            if (auto* f = syncFilterStateAdaptive(&filter, target_state, core_value_for_bypass, neutral_val_for_bypass)) {
                lerpParamBallistics(f, "level", targets.speech_enhancement_level, 0.1f, false);
            }
        } else if (filter.id == "f_noise_reduction") {
            target_state = m_specializedFilterState.noise_reduction;
            core_value_for_bypass = filter.params.count("l") ? filter.params.at("l").current : 0.0f;
            neutral_val_for_bypass = 0.0f; // Mức trung tính cho giảm nhiễu
            if (auto* f = syncFilterStateAdaptive(&filter, target_state, core_value_for_bypass, neutral_val_for_bypass)) {
                lerpParamBallistics(f, "l", targets.noise_reduction_level, 0.1f, false);
            }
        } else if (filter.id == "f_audio_restoration") {
            target_state = m_specializedFilterState.audio_restoration || targets.audio_restoration_enabled;
            // Đối với declick, không có tham số cốt lõi để lerp, chỉ bật/tắt
            core_value_for_bypass = filter.enabled ? 1.0f : 0.0f; // Nếu bật, coi là "active"
            neutral_val_for_bypass = 0.0f; // "Inactive"
            syncFilterStateAdaptive(&filter, target_state, core_value_for_bypass, neutral_val_for_bypass);
        } else if (filter.id == "f_vocal_remover") { // Bộ lọc tách lời
            target_state = targets.vocal_remover_enabled;
            core_value_for_bypass = filter.params.count("mode") ? filter.params.at("mode").current : 0.0f;
            neutral_val_for_bypass = 0.0f; // Chế độ trung tính cho tách lời
            if (auto* f = syncFilterStateAdaptive(&filter, target_state, core_value_for_bypass, neutral_val_for_bypass)) {
                lerpParamBallistics(f, "mode", targets.vocal_remover_mode, 0.2f, false);
            }
        }
    }

    // 5. Chỉ đẩy dữ liệu đi khi thực sự có biến động cấu trúc hoặc tham số thay đổi
    if (need_sync_structure || parameter_changed) {
        DispatchParametersToMPV(need_sync_structure, parameter_changed);
    }

    PublishControlSnapshot();
}