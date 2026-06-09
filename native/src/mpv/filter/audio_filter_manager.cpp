#include <mpv/filter/audio_filter_manager.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip> // Thêm để format số float cho mpv

AudioFilterManager& AudioFilterManager::Instance() {
    static AudioFilterManager instance;
    return instance;
}

void AudioFilterManager::Init(mpv_handle* h) { 
    mpv = h; 
    // Sửa lại tên file cấu hình cho đúng chính tả tiếng Anh (filter thay vì fillter)
    path = AutoPath<std::string>("%ROOT%", "data", "audio_filter.json");
    
    // Mặc định kênh âm thanh ban đầu
    m_channelMode = "stereo";

    // =========================================================================
    // 1. Nhóm Equalizer (Cân bằng âm sắc)
    // =========================================================================
    // g (gain): giới hạn chuẩn của FFmpeg bass/treble là -20dB đến +20dB. Quá mức sẽ gây méo tiếng nghiêm trọng.
    // f (frequency): Bass thường xử lý dải âm trầm thấp dưới 300Hz.
    AddFilter("f_bass", "bass", "eq");
    RegisterParam("f_bass", "g", -20.0f, 20.0f, 0.0f);     // Gain (dB)
    RegisterParam("f_bass", "f", 20.0f, 500.0f, 100.0f);   // Cắt tần số Bass (Hz)

    // Treble xử lý dải âm cao. Thường từ 1000Hz (1kHz) đến ngưỡng nghe 20000Hz (20kHz).
    AddFilter("f_treble", "treble", "eq");
    RegisterParam("f_treble", "g", -20.0f, 20.0f, 0.0f);   // Gain (dB)
    RegisterParam("f_treble", "f", 1000.0f, 20000.0f, 3500.0f); // Cắt tần số Treble (Hz)

    // Parametric Equalizer tinh chỉnh dải tần hẹp trung tâm
    // w (width): Độ rộng băng tần (Bandwidth), mặc định thường dùng Octave. 
    // Giới hạn an toàn từ 0.1 đến 10.0 (Octave). Mặc định là 1.0.
    AddFilter("f_eq", "equalizer", "eq");
    RegisterParam("f_eq", "f", 20.0f, 20000.0f, 1000.0f);  // Tần số trung tâm (Hz)
    RegisterParam("f_eq", "w", 0.1f, 10.0f, 1.0f);         // Độ rộng băng tần (Q-factor / Width)
    RegisterParam("f_eq", "g", -20.0f, 20.0f, 0.0f);       // Gain (dB)
    

    // =========================================================================
    // 2. Nhóm Dynamic Range (Kiểm soát cường độ / Âm lượng)
    // =========================================================================
    // volume: Biên độ âm lượng tính bằng dB. Không nên để max quá +30dB tránh clipping (bể tiếng).
    AddFilter("f_volume", "volume", "gain");
    RegisterParam("f_volume", "volume", -30.0f, 30.0f, 0.0f); // Volume (dB)

    // acompressor: Bộ nén tiếng giúp cân bằng các đoạn âm thanh quá to và quá nhỏ
    // threshold: Sẽ được hàm GetInitString chuyển đổi tự động từ dB sang Linear [0.00001 - 1.0]
    // ratio: Tỷ lệ nén. 1.0 (không nén) -> 20.0 (nén cực mạnh như Limiter).
    AddFilter("f_comp", "acompressor", "dynamics");
    RegisterParam("f_comp", "threshold", -60.0f, 0.0f, -12.0f); // Ngưỡng nén (Hệ dB trực quan trên UI)
    RegisterParam("f_comp", "ratio", 1.0f, 20.0f, 2.0f);         // Tỷ lệ nén (x:1)
    RegisterParam("f_comp", "attack", 0.01f, 2000.0f, 20.0f);    // Attack time (ms)
    RegisterParam("f_comp", "release", 0.01f, 9000.0f, 250.0f);  // Release time (ms)

    // loudnorm: Chuẩn hóa âm thanh theo tiêu chuẩn EBU R128 (Không có tham số động, bật/tắt là chạy)
    AddFilter("f_norm", "loudnorm", "dynamics");

    // =========================================================================
    // 3. Nhóm Không gian âm thanh (Spatial / Stereo Expansion)
    // =========================================================================
    // extrastereo: Hiệu ứng mở rộng không gian Stereo. 
    // m (coefficient): Giá trị từ 0.0 (Mono) đến 10.0 (Mở rộng tối đa). Mặc định 2.5.
    AddFilter("f_stereo", "extrastereo", "spatial");
    RegisterParam("f_stereo", "m", 0.0f, 10.0f, 2.5f); 

    // crystalizer: Tăng cường độ chi tiết/độ động của các file âm thanh nén nát (như MP3 chất lượng thấp)
    // i (intensity): Cường độ xử lý từ 0.0 đến 10.0. Mặc định 2.0.
    AddFilter("f_crystalizer", "crystalizer", "spatial");
    RegisterParam("f_crystalizer", "i", 0.0f, 10.0f, 2.0f); 

    // bs2b: Bộ lọc chuyển đổi âm thanh từ Tai nghe sang giả lập Loa thùng (Bauer stereophonic-to-binaural)
    // profile: FFmpeg chấp nhận các cấu hình cố định tương ứng: 0 (default), 1 (cmoy), 2 (jmeier).
    AddFilter("f_bs2b", "bs2b", "spatial"); 
    RegisterParam("f_bs2b", "profile", 0.0f, 2.0f, 0.0f); 

    // =========================================================================
    // 4. Nhóm Tốc độ và Cao độ (Time/Pitch) -> Sử dụng bộ lọc ATEMPO đa nền tảng
    // =========================================================================
    // Do hệ thống libmpv của bạn bị khuyết thư viện mã nguồn mở librubberband,
    // ta chuyển hẳn sang dùng atempo mặc định của core FFmpeg, tuyệt đối an toàn và ổn định.
    AddFilter("f_pitch", "atempo", "time_pitch");
    RegisterParam("f_pitch", "tempo", 0.5f, 2.0f, 1.0f); // Điều chỉnh tốc độ từ 0.5x đến 2.0x

    // =========================================================================
    // 5. Nhóm Hiệu ứng (Effects / Modulations)
    // =========================================================================
    // chorus: Hiệu ứng đồng ca. Đã được bọc cứng chuỗi mảng bắt buộc (delays, decays, speeds...)
    // trong GetInitString(). Trên UI chỉ hiển thị 2 slider gain điều khiển cực kỳ an toàn.
    AddFilter("f_chorus", "chorus", "effects");
    RegisterParam("f_chorus", "in_gain", 0.0f, 1.0f, 0.4f);
    RegisterParam("f_chorus", "out_gain", 0.0f, 1.0f, 0.4f);

    // flanger: Hiệu ứng âm thanh phản phất / biến đổi chu kỳ thời gian trì hoãn.
    // LƯU Ý KỸ THUẬT: Giống như chorus, flanger yêu cầu gán chuỗi thông số rất nghiêm ngặt.
    // Ta đăng ký 2 tham số chính là delay (trễ nền) và depth (độ sâu biến thiên), các tham số phụ
    // như lfo, speed, feedback sẽ được xử lý chuỗi nội bộ tương tự để tránh lỗi "option not found".
    AddFilter("f_flanger", "flanger", "effects");
    RegisterParam("f_flanger", "delay", 0.0f, 30.0f, 0.0f);  // Base delay (ms)
    RegisterParam("f_flanger", "depth", 0.0f, 10.0f, 2.0f);  // Swept delay thickness (ms)
}
void AudioFilterManager::AddFilter(const std::string& id, const std::string& name, const std::string& group) {
    m_filters.push_back({id, name, false, {}, group});
    m_filterIndex[id] = m_filters.size() - 1; 
}
void AudioFilterManager::SetFilterEnabled(const std::string& id, bool enabled) {
    if (auto* f = FindFilter(id)) {
        if (f->enabled != enabled) { 
            f->enabled = enabled;

            // Nếu bộ lọc này được BẬT và nó thuộc một nhóm xung đột cụ thể
            if (enabled && !f->group.empty()) {
                for (auto& other : m_filters) {
                    // Tắt tất cả các filter khác cùng nhóm, ngoại trừ chính nó
                    if (other.id != id && other.group == f->group && other.enabled) {
                        other.enabled = false;
                        // Bạn có thể log ra console hoặc bắn signal báo cho UI biết
                        std::cout << "[Conflict System] Tắt tự động " << other.id << " do xung đột với nhóm: " << f->group << "\n";
                    }
                }
            }
            SyncAll(); 
        }
    }
}
void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def) {
    if (auto* f = FindFilter(id)) {
        f->params[key] = {def, min, max, def}; 
    }
}

void AudioFilterManager::ResetFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        for (auto& [key, p] : f->params) {
            p.current = p.def;
        }
        if (f->enabled) SyncAll();
    }
}

void AudioFilterManager::SaveToFile() {
    if (path.empty()) return; // Bảo vệ an toàn đường dẫn
    std::ofstream f(path);
    if (!f.is_open()) return;
    for (const auto& filter : m_filters) {
        f << "[Filter]:" << filter.id << "|" << (filter.enabled ? "1" : "0") << "\n";
        for (const auto& [key, p] : filter.params) {
            f << key << ":" << p.current << "\n";
        }
    }
}

void AudioFilterManager::ResetAllToDefaults() {
    for (auto& filter : m_filters) {
        filter.enabled = false; 
        for (auto& [key, p] : filter.params) {
            p.current = p.def; 
        }
    }
    m_channelMode = "stereo"; 
    SyncAll(); 
}

void AudioFilterManager::LoadFromFile() {
    if (path.empty()) { ResetAllToDefaults(); return; }
    
    std::ifstream f(path);
    if (!f.is_open()) {
        ResetAllToDefaults(); 
        return; 
    }

    std::string line;
    std::string current_id = "";
    bool parse_success = false;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        if (line.rfind("[Filter]:", 0) == 0) {
            size_t delim = line.find('|');
            if (delim != std::string::npos) {
                current_id = line.substr(9, delim - 9);
                bool enabled = (line.substr(delim + 1) == "1");
                if (auto* filter = FindFilter(current_id)) {
                    filter->enabled = enabled;
                    parse_success = true; // Đánh dấu đã đọc được ít nhất 1 filter hợp lệ
                } else {
                    current_id = ""; // Xoá ID nếu ID này không tồn tại trong hệ thống
                }
            }
        } else if (!current_id.empty()) {
            size_t delim = line.find(':');
            if (delim != std::string::npos) {
                std::string key = line.substr(0, delim);
                try {
                    float val = std::stof(line.substr(delim + 1));
                    if (auto* filter = FindFilter(current_id)) {
                        if (filter->params.count(key)) {
                            // Giới hạn giá trị đọc từ file vào khoảng min-max để an toàn
                            auto& param = filter->params[key];
                            param.current = (val < param.min) ? param.min : (val > param.max ? param.max : val);
                        }
                    }
                } catch (...) {
                    continue; // Bỏ qua dòng lỗi dữ liệu số
                }
            }
        }
    }

    if (!parse_success) {
        ResetAllToDefaults();
    } else {
        SyncAll(); 
    }
}

void AudioFilterManager::SyncAll() {
    if (!mpv) return;
    
    std::string full_af = "";

    // Cấu hình Kênh âm thanh ban đầu
    if (m_channelMode == "mono") {
        full_af += "pan=mono|c0=0.5*c0+0.5*c1";
    } else if (m_channelMode == "surround") {
        full_af += "pan=5.1|FL=c0|FR=c1|FC=c0+c1|LFE=0|BL=c0|BR=c1";
    }

    // Duyệt qua các audio filter thông thường
    for (const auto& f : m_filters) {
        if (f.enabled) {
            if (!full_af.empty()) full_af += ",";
            full_af += f.GetInitString(); 
        }
    }
    
    mpv_set_property_string(mpv, "af", full_af.c_str());
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    if (auto* f = FindFilter(id)) {
        auto it = f->params.find(key);
        if (it != f->params.end()) { 
            auto& param = it->second;

            // Clamp giá trị
            if (value < param.min) value = param.min;
            if (value > param.max) value = param.max;
            
            param.current = value;
            
            // Cập nhật Realtime xuống MPV
            if (f->enabled && mpv) {
                // Khắc phục lỗi format float của std::to_string bằng std::ostringstream
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << value; 
                std::string val_str = ss.str();

                const char* cmd[] = {"af-command", id.c_str(), key.c_str(), val_str.c_str(), NULL};
                mpv_command(mpv, cmd);
            }
        }
    }
}

void AudioFilterManager::BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates) {
    bool has_changed = false;
    for (const auto& [id, key, value] : updates) {
        if (auto* f = FindFilter(id)) {
            auto it = f->params.find(key);
            if (it != f->params.end()) {
                auto& param = it->second;
                float clamped = value;
                if (clamped < param.min) clamped = param.min;
                if (clamped > param.max) clamped = param.max;
                
                if (param.current != clamped) {
                    param.current = clamped;
                    has_changed = true;
                }
            }
        }
    }
    if (has_changed) {
        SyncAll(); 
    }
}

void AudioFilterManager::ToggleFilter(const std::string& id, bool state) {
    SetFilterEnabled(id, state); 
}


void AudioFilterManager::SetAllFiltersState(bool enabled) {
    bool changed = false;
    for (auto& f : m_filters) {
        if (f.enabled != enabled) {
            f.enabled = enabled;
            changed = true;
        }
    }
    if (changed) {
        SyncAll();
    }
}

bool AudioFilterManager::IsFilterEnabled(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        return f->enabled;
    }
    return false;
}

int AudioFilterManager::GetActiveFilterCount() {
    int count = 0;
    for (const auto& f : m_filters) {
        if (f.enabled) count++;
    }
    return count;
}

AudioFilter* AudioFilterManager::FindFilter(const std::string& id) {
    auto it = m_filterIndex.find(id);
    if (it != m_filterIndex.end() && it->second < m_filters.size()) {
        return &m_filters[it->second];
    }
    return nullptr;
}

void AudioFilterManager::AddAudioTrack(const std::string& trackId, const std::string& lang, const std::string& codec) {
    m_audioTracks.push_back({trackId + " [" + lang + "] (" + codec + ")", trackId});
}

void AudioFilterManager::SelectAudioTrack(const std::string& trackId) {
    if (!mpv) return;
    m_currentAudioTrack = trackId;
    
    // 🌟 SỬA LỖI: Sử dụng mpv_set_property_string điều khiển thuộc tính "aid" thay vì mpv_command sai cú pháp
    mpv_set_property_string(mpv, "aid", trackId.c_str());
}

void AudioFilterManager::SetChannelMode(const std::string& mode) {
    if (m_channelMode != mode) {
        m_channelMode = mode;
        
        // Nếu chuyển sang chế độ "mono", tự động tắt các filter xử lý stereo không gian
        if (mode == "mono") {
            if (auto* f = FindFilter("f_stereo")) f->enabled = false;
            if (auto* f = FindFilter("f_bs2b")) f->enabled = false;
        }
        SyncAll(); 
    }
}