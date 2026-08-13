#include "af_m.h"

void AudioFilterManager::DispatchParametersToMPV(bool need_sync_structure, bool parameter_changed) {
    if (need_sync_structure) {
        EvaluateSystemSafety();
        SyncAll(); 
    } 
    else if (parameter_changed) {
        EvaluateSystemSafety();

        for (auto& filter : m_filters) {
            if (filter.enabled) {
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

                    char val_str[32];
                    if ((filter.name == "acompressor" && (key == "threshold" || key == "makeup")) || filter.id == "f_vol_booster") {
                        snprintf(val_str, sizeof(val_str), "%.5f", value_to_send);
                    } else {
                        snprintf(val_str, sizeof(val_str), "%.2f", value_to_send);
                    }

                    // Xử lý đặc biệt cho f_vocal_remover
                    if (filter.id == "f_vocal_remover" && key == "mode") {
                        const char* mode_str = p.current > 0.5f ? "l-r" : "lr>lr"; // lr>lr là chế độ mặc định (không làm gì)
                        const char* cmd[] = {"af-command", filter.id.c_str(), key.c_str(), mode_str, NULL};
                        mpv_command(mpv, cmd);
                        p.lastSent = p.current;
                        continue; // Bỏ qua phần còn lại của vòng lặp
                    }

                    const char* cmd[] = {"af-command", filter.id.c_str(), key.c_str(), val_str , NULL};
                    mpv_command(mpv, cmd);

                    p.lastSent = p.current;
                }
            }
        }
    }
}