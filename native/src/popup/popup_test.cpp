#include "popup_test.h"

#include "WindowManager.h"

#include "gui.h"
#include "player/session/PlayerSession.h"
#include "windows/WindowRuntime.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib> // Cần cho hàm rand() mô phỏng noise floor
#include "log.h"

// Helper để chuyển enum thành chuỗi
const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:     return "Info";
        case LogLevel::Warning:  return "Warning";
        case LogLevel::Error:    return "Error";
        case LogLevel::Debug:    return "Debug";
        case LogLevel::Critical: return "Critical";
        default:                 return "Unknown";
    }
}

std::string LogCategoryToString(LogCategory category) {
    if (category == LogCategory::None) return "None";
    std::string result;
    if ((static_cast<uint32_t>(category) & static_cast<uint32_t>(LogCategory::System))) result += "System ";
    if ((static_cast<uint32_t>(category) & static_cast<uint32_t>(LogCategory::AI))) result += "AI ";
    if ((static_cast<uint32_t>(category) & static_cast<uint32_t>(LogCategory::Audio))) result += "Audio ";
    // Thêm các category khác nếu cần
    if (!result.empty()) result.pop_back();
    return result;
}
auto& logManager = LogHistoryManager::GetInstance();
// Cấu trúc Logger Realtime để theo dõi sự kiện nội bộ UI
struct LocalLogger {
    bool ScrollToBottom = false;

    void Clear() {logManager.Clear();}

    void Draw() {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Nhật ký gọi lệnh & Hệ thống AI (Realtime Logs) ---");

        // --- BỘ LỌC CATEGORY ---
        static uint32_t category_mask = static_cast<uint32_t>(LogCategory::All);
        ImGui::Text("Filter by Category: ");
        ImGui::SameLine();
        ImGui::CheckboxFlags("Sys", &category_mask, static_cast<uint32_t>(LogCategory::System)); ImGui::SameLine();
        ImGui::CheckboxFlags("AI", &category_mask, static_cast<uint32_t>(LogCategory::AI)); ImGui::SameLine();
        ImGui::CheckboxFlags("Audio", &category_mask, static_cast<uint32_t>(LogCategory::Audio)); ImGui::SameLine();
        ImGui::CheckboxFlags("Render", &category_mask, static_cast<uint32_t>(LogCategory::Render)); ImGui::SameLine();
        ImGui::CheckboxFlags("UI", &category_mask, static_cast<uint32_t>(LogCategory::UI)); ImGui::SameLine();
        ImGui::CheckboxFlags("Sync", &category_mask, static_cast<uint32_t>(LogCategory::Sync)); ImGui::SameLine();
        ImGui::CheckboxFlags("Safety", &category_mask, static_cast<uint32_t>(LogCategory::Safety));
        if (ImGui::Button("All")) category_mask = static_cast<uint32_t>(LogCategory::All);
        ImGui::SameLine();
        if (ImGui::Button("None")) category_mask = static_cast<uint32_t>(LogCategory::None);


        ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        
        // 2. LẤY REAL-TIME LOGS TỪ LÕI MANAGER (Đã sửa lỗi đồng bộ, lấy trực tiếp trong frame vẽ)
        auto core_logs = logManager.GetRecentLogs(static_cast<LogCategory>(category_mask));
        if(!core_logs.empty()) {
            for (const auto& log : core_logs) {
                ImVec4 color;
                switch (log.level) {
                    case LogLevel::Warning:  color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); break;
                    case LogLevel::Error:    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
                    case LogLevel::Critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
                    case LogLevel::Debug:    color = ImVec4(0.5f, 0.5f, 1.0f, 1.0f); break;
                    default:                 color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); break;
                }

                // Chuyển đổi timestamp sang định dạng giờ:phút:giây
                auto time_t_now = std::chrono::system_clock::to_time_t(log.timestamp);
                std::tm buf;
                localtime_s(&buf, &time_t_now);
                std::ostringstream time_ss;
                time_ss << std::put_time(&buf, "%H:%M:%S");

                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", time_ss.str().c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "[%s]", LogLevelToString(log.level));
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1.0f), "[%s]", LogCategoryToString(log.category).c_str());
                ImGui::SameLine();
                ImGui::TextColored(color, "%s", log.message.c_str());
            }
        }
        if (ScrollToBottom) { 
            ImGui::SetScrollHereY(1.0f); 
            ScrollToBottom = false; 
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
};

static LocalLogger g_PopupLogger;

static void DrawAudioVisualizer(
    PlayerSession &session)
{
    AudioVisualizerFrame frame;
    session.GetAudio()->GetVisualizerData(frame);
    CSImGui::ModernHeader("Realtime Audio Visualizer");

    if (CSImGui::BeginCard()){

        CSImGui::ModernHeader("Signal Level");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Peak L: %.4f",
            frame.peakLeft
        );
        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Peak R: %.4f",
            frame.peakRight
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "RMS L: %.4f",
            frame.rmsLeft
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "RMS R: %.4f",
            frame.rmsRight
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Crest Factor L: %.3f",
            frame.crestFactorLeft
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Crest Factor R: %.3f",
            frame.crestFactorRight
        );

        CSImGui::EndCard();
    }

    if (CSImGui::BeginCard())
    {
        CSImGui::ModernHeader("Loudness & Dynamic Range");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Short-term LUFS: %.2f",
            frame.shortTermLUFS
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Dynamic Range: %.2f dB",
            frame.dynamicRange
        );

        CSImGui::EndCard();
    }

    if (CSImGui::BeginCard())
    {
        CSImGui::ModernHeader("Signal Integrity");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Clipping Left: %s",
            frame.isClippingLeft ? "YES" : "NO"
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Clipping Right: %s",
            frame.isClippingRight ? "YES" : "NO"
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Clip Count: %u",
            frame.clipCount
        );

        CSImGui::EndCard();
    }

    if (CSImGui::BeginCard())
    {
        CSImGui::ModernHeader("Frequency Energy");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Sub Bass 20-60 Hz: %.4f",
            frame.subBassEnergy
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Bass 60-250 Hz: %.4f",
            frame.bassEnergy
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Mid 250-4k Hz: %.4f",
            frame.midEnergy
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Treble 4k-20k Hz: %.4f",
            frame.trebleEnergy
        );

        CSImGui::EndCard();
    }


    if (CSImGui::BeginCard())
    {
        CSImGui::ModernHeader("Stereo / Phase");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Phase Correlation: %.4f",
            frame.phaseCorrelation
        );

        CSImGui::EndCard();
    }

    if (CSImGui::BeginCard())
    {
        CSImGui::ModernHeader("Sync Metadata");

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "PTS: %.6f",
            frame.pts
        );

        CSImGui::ModernTextEffectFmt(
            TextEffectStyle{},
            "Sequence: %llu",
            static_cast<unsigned long long>(frame.sequence)
        );

        CSImGui::EndCard();
    }
}

void DrawCoreMonitor(
    AudioFilterManager& manager,
    const AudioContext& ctx)
{
    if (!CSImGui::ModernCollapsingHeader(
            "1. Giám sát hệ thống Core & Phân tích Track Audio",
            ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    // ============================================================

    if (CSImGui::BeginInfoTable(
            "TrackAnalysisInfo",
            2,
            140.0f))
    {
        CSImGui::InfoRow(
            "Volume",
            "%.1f%%",
            ctx.volume);

        CSImGui::InfoRow(
            "Sample Rate",
            "%lld Hz",
            ctx.sample_rate);

        CSImGui::InfoRow(
            "Số Kênh",
            "%lld Ch",
            ctx.channel_count);

        CSImGui::InfoRow(
            "Bitrate",
            "%.1f kbps",
            ctx.bitrate_kbps);

        CSImGui::EndInfoTable();
    }


    // ============================================================
    // 2. Global Audio Filter State
    // ============================================================

    if (CSImGui::BeginInfoTable(
            "GlobalAudioState",
            2,
            140.0f))
    {
        CSImGui::InfoRow(
            "Kênh Output",
            "%s",
            manager.GetChannelMode().c_str());

        CSImGui::InfoRow(
            "Filter Hoạt Động",
            "%d",
            manager.GetActiveFilterCount());

        CSImGui::InfoRow(
            "Quản Lý An Toàn",
            "%s",
            manager.IsGlobalBypassEnabled()
                ? "BYPASS"
                : "ACTIVE");

        CSImGui::EndInfoTable();
    }


    // ============================================================
    // 3. Filter Matrix
    // ============================================================

    const std::vector<TableCol> columns = {
        { "ID Node",                         110.0f },
        { "Bộ Lọc FFmpeg",                  140.0f },
        { "Nhóm",                            120.0f },
        { "Trạng Thái",                     150.0f },
        { "Giá Trị Bộ Nhớ Realtime (Key: Value)", 0.0f }
    };

    if (!CSImGui::BeginListTable(
            "ManagerInternalTable",
            columns))
    {
        return;
    }


    // ============================================================
    // 4. Filter Rows
    // ============================================================

    for (const auto& f : manager.GetFilters())
    {
        bool is_row_selected = false; // Hoặc bind với biến state của bạn

        if (!CSImGui::BeginListRow(f.id.c_str(), is_row_selected))
        {
            continue;
        }

        // Cột 1: ID Node (Đã ở TableNextColumn() sẵn trong BeginListRow)
        ImGui::TextUnformatted(f.id.c_str());

        // Cột 2: Bộ Lọc FFmpeg
        CSImGui::TableText(f.name.c_str());

        // Cột 3: Nhóm
        if (f.group.empty()) {
            CSImGui::TableTextDisabled("None");
        } else {
            CSImGui::TableText(f.group.c_str());
        }

        // Cột 4: Trạng Thái
        if (f.enabled) {
            CSImGui::TableStatus("ENABLED", StatusType::Success);
        } else {
            CSImGui::TableStatus("DISABLED", StatusType::Disabled); // Hoặc StatusType::None / Error tùy bạn định nghĩa
        }

        // Nếu có bypass management, vẽ Badge phụ hoặc tag ngay kế bên
        if (f.isBypassManagement) {
            ImGui::SameLine();
            CSImGui::WarningBadge("BYPASS");
        }

        // Cột 5: Realtime Params
        ImGui::TableNextColumn();
        if (f.params.empty()) {
            ImGui::TextDisabled("Không cấu hình tham số.");
        } else {
            std::string param_dump;
            for (const auto& [key, p] : f.params) {
                std::ostringstream ss;
                ss << key << ": [" << std::fixed << std::setprecision(2) << p.current << "]  ";
                param_dump += ss.str();
            }
            ImGui::TextWrapped("%s", param_dump.c_str());
        }

        CSImGui::EndListRow();
    }

    // ============================================================
    // 5. End Table
    // ============================================================
    CSImGui::EndListTable();
}

void DrawAdaptiveControl(
    AudioFilterManager& manager,
    const AudioContext& ctx)
{   
    if (CSImGui::ModernCollapsingHeader("2. Trợ lý AI & Điều khiển Vượt tuyến (Adaptive Matrix)")) {
        
        // --- KHỐI ĐIỀU KHIỂN BIÊN ĐỘ QUẢN LÝ (GLOBAL BYPASS) ---
        bool globalBypass = manager.IsGlobalBypassEnabled();
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 0.4f, 0.0f, 0.4f));
        if (ImGui::Checkbox("HỦY BỎ TOÀN BỘ HỆ THỐNG QUẢN LÝ (Global Bypass Security & Conflicts)", &globalBypass)) {
            manager.SetGlobalBypassMode(globalBypass);
        }
        ImGui::PopStyleColor();
        
        ImGui::Spacing();

        // --- KHỐI QUẢN LÝ ĐỘC LẬP NGOẠI VI (ỔN ĐỊNH MẠCH VÀ TĂNG CƯỜNG ÂM LƯỢNG) ---
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Cơ cấu Bảo vệ & Kích âm Ngoại vi độc lập:");
        ImGui::Indent(10.0f);
        
        if (ImGui::Checkbox("Kích hoạt bộ Ổn định ngoại vi độc lập (f_out_compressor & f_out_limiter)", &manager.m_enableOuterStabilizer)) {
            manager.SetOuterStabilizerEnabled(manager.m_enableOuterStabilizer);
        }
        
        ImGui::SameLine();
        ImGui::Spacing(); ImGui::SameLine();

        if (ImGui::Checkbox("Kích hoạt mạch Kích âm độc lập (f_vol_booster)", &manager.m_enableOuterBooster)) {
            manager.SetOuterBoosterEnabled(manager.m_enableOuterBooster);
        }
        ImGui::Unindent(10.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- KHỐI ĐIỀU KHIỂN TRỢ LÝ THÔNG MINH (ADAPTIVE AI) ---
        bool autoMode = manager.IsAdaptiveMode();
        bool wasAutoMode = autoMode; 

        if (wasAutoMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
        }

        if (ImGui::Checkbox("KÍCH HOẠT ĐIỀU CHỈNH CHẤT ÂM TỰ ĐỘNG (Adaptive AI Engine)", &autoMode)) {
            manager.SetAdaptiveMode(autoMode, manager.GetCurrentPreset());
        }

        if (wasAutoMode) {
            ImGui::PopStyleColor(2);
        }

        if (autoMode) {
            ImGui::Indent(25.0f);
            const char* presetNames[] = { 
                "Cân bằng Studio (Flat)", 
                "Nhạc Trẻ / Pop Vocal", 
                "Heavy Rock / Metal", 
                "EDM / Dance Floor", 
                "Cổ điển / Classical",
                "Nhạc Mộc / Acoustic Guitar",
                "Chế độ Gaming (FPS Footsteps)",
                "Điện ảnh / Movie Cinema",
                "Siêu Trầm / Deep Bass"
            };
            int currentPresetIdx = static_cast<int>(manager.GetCurrentPreset());
            
            ImGui::SetNextItemWidth(280.0f);
            if (ImGui::Combo("Phong cách âm nhạc chủ đạo", &currentPresetIdx, presetNames, IM_ARRAYSIZE(presetNames))) {
                manager.SetCurrentPreset(static_cast<AudioPreset>(currentPresetIdx));
            }
            
            // Ghi Log tự động ra Monitor nếu có sự biến động lớn từ track
            static double last_logged_vol = 0.0;
            if (std::abs(ctx.volume - last_logged_vol) > 15.0) {
                last_logged_vol = ctx.volume;
            }
            ImGui::Unindent(25.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- DANH SÁCH CHI TIẾT TỪNG FILTER VÀ CÁC NÚT KHÓA TAY TỪNG PHẦN ---
        if (ImGui::Button("Bật toàn bộ hệ thống")) {
            manager.SetAllFiltersState(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Tắt toàn bộ hệ thống")) {
            manager.SetAllFiltersState(false);
        }

        ImGui::Spacing();

        for (const auto& filterRef : manager.GetFilters()) {
            auto* f = manager.FindFilter(filterRef.id);
            if (!f) continue;

            ImGui::PushID(f->id.c_str());

            // Checkbox điều khiển cơ bản từng node
            bool isEnabled = f->enabled;
            if (ImGui::Checkbox("##toggle", &isEnabled)) {
                manager.ToggleFilter(f->id, isEnabled);
            }
            
            ImGui::SameLine();
            
            // Nút Quản lý Vượt tuyến (Bypass Node) - Cho phép giành lại quyền điều khiển từ AI
            if (f->isBypassManagement) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
                if (ImGui::Button("Mở Khóa AI")) {
                    manager.SetFilterBypassMode(f->id, false);
                }
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button(" Khóa Tay ")) {
                    manager.SetFilterBypassMode(f->id, true);
                }
            }

            ImGui::SameLine();
            
            // Đổi màu hiển thị tiêu đề node dựa theo trạng thái
            if (f->enabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "[%s] Node %s (Group: %s)", f->id.c_str(), f->name.c_str(), f->group.c_str());
            } else {
                ImGui::TextDisabled("[%s] Node %s", f->id.c_str(), f->name.c_str());
            }

            // Vùng vẽ Slider tinh chỉnh cho từng tham số bên trong Filter
            if (!f->params.empty()) {
                ImGui::Indent(35.0f);
                
                // Nếu đang bật Adaptive AI điều khiển tự động và node này KHÔNG được chọn khóa tay -> Khóa các thanh kéo Slider
                bool lockWidgets = autoMode && !f->isBypassManagement;
                if (lockWidgets) ImGui::BeginDisabled();

                for (auto& [key, param] : f->params) {
                    float val = param.current;
                    std::string label = key + "##" + f->id;
                    
                    const char* format = "%.2f";
                    if (param.max > 1000.0f) format = "%.0f Hz";
                    else if (key == "g" || key == "volume" || key == "threshold") format = "%.1f dB";

                    if (ImGui::SliderFloat(label.c_str(), &val, param.min, param.max, format)) {
                        manager.UpdateParam(f->id, key, val);
                    }
                }

                if (lockWidgets) ImGui::EndDisabled();

                if (!lockWidgets) {
                    ImGui::SameLine();
                    if (ImGui::Button("Reset Node")) {
                        manager.ResetFilter(f->id);

                    }
                }
                ImGui::Unindent(35.0f);
            }
            
            ImGui::PopID();
            ImGui::Separator();
        }

        // --- Cấu hình Kênh & File I/O ---
        ImGui::Spacing();
        ImGui::Text("Cấu hình Luồng đầu ra (Channel Matrix):");
        const char* modes[] = { "stereo", "mono", "surround" };
        for (int n = 0; n < 3; n++) {
            if (ImGui::Button(modes[n])) {
                manager.SetChannelMode(modes[n]);
            }
            if (n < 2) ImGui::SameLine();
        }

        ImGui::Spacing();
        if (ImGui::Button("Lưu Cấu Hình (Save To Disk)")) {
            manager.SaveToFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Tải Cấu Hình (Load From Disk)")) {
            manager.LoadFromFile();
        }
    }
}

void DrawEBUR128Monitor(
    AudioFilterManager& manager,
    const AudioContext& ctx)
{
        if (ImGui::CollapsingHeader("3. Radar chẩn đoán Động học & Âm lượng nâng cao (EBU R128)", ImGuiTreeNodeFlags_DefaultOpen)) {
        
        // --- 1. QUẢN LÝ TRẠNG THÁI ĐỂ CHỐNG LẶP LOGS (STATE-MACHINE LOG ENGINE) ---
        enum class PeakState { SAFE, WARNING, CLIPPING };
        static PeakState last_peak_state = PeakState::SAFE;
        static bool last_hot_state = false;
        
        // Xác định trạng thái hiện tại dựa trên dữ liệu engine
        PeakState current_peak_state = PeakState::SAFE;
        if (ctx.true_peak_ch0 >= 1.0 || ctx.true_peak_ch1 >= 1.0) {
            current_peak_state = PeakState::CLIPPING;
        } else if (ctx.true_peak_ch0 > 0.95 || ctx.true_peak_ch1 > 0.95) {
            current_peak_state = PeakState::WARNING;
        }
        bool current_hot_state = (ctx.loudness_shortterm > -5.0);



        // --- THỐNG KÊ METADATA CƠ BẢN ---
        ImGui::Columns(3, "EbuMetaGrid", false);
        ImGui::Text("Codec Hiện Tại: %s", ctx.codec.empty() ? "N/A" : ctx.codec.c_str()); ImGui::NextColumn();
        ImGui::Text("Audio Bitrate: %.1f kbps", ctx.bitrate_kbps); ImGui::NextColumn();
        ImGui::Text("Chế độ Stream: %s", ctx.is_audio_only ? "Chỉ Audio" : "Video + Audio Stream");
        ImGui::Columns(1);
        ImGui::Separator();

        // --- HỆ THỐNG ĐỒ THỊ CUỘN TIME-SERIES CHO LOUDNESS ---
        const int HISTORY_SIZE = 128;
        static float momentary_history[HISTORY_SIZE] = { -70.0f };
        static float shortterm_history[HISTORY_SIZE] = { -70.0f };
        static int history_offset = 0;

        momentary_history[history_offset] = static_cast<float>(ctx.loudness_momentary);
        shortterm_history[history_offset] = static_cast<float>(ctx.loudness_shortterm);
        history_offset = (history_offset + 1) % HISTORY_SIZE;

        ImGui::Spacing();
        ImGui::Columns(2, "LoudnessGraphsGrid", true);

        ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Độ to Tức thời (Momentary - 400ms)");
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
        ImGui::PlotLines("##M_LoudnessPlot", momentary_history, HISTORY_SIZE, history_offset, nullptr, -60.0f, 0.0f, ImVec2(0, 90));
        ImGui::PopStyleColor();
        ImGui::Text("Hiện tại: %.1f LUFS", ctx.loudness_momentary);

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Độ to Ngắn hạn (Short-term - 3s)");
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        ImGui::PlotLines("##S_LoudnessPlot", shortterm_history, HISTORY_SIZE, history_offset, nullptr, -60.0f, 0.0f, ImVec2(0, 90));
        ImGui::PopStyleColor();
        ImGui::Text("Hiện tại: %.1f LUFS", ctx.loudness_shortterm);

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();

        // --- KHỐI GIÁM SÁT ĐỈNH SÓNG THỰC (LEVEL METERS) ---
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Đỉnh Sóng Thực Tuyến Tính (True Peak Level Meters):");
        
        char peak_text_buffer[64];
        float available_width = ImGui::GetContentRegionAvail().x;

        ImGui::Text("Ch 0 [L]:"); ImGui::SameLine(75.0f); 
        ImVec4 ch0_color = (ctx.true_peak_ch0 > 0.95) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ch0_color);
        sprintf(peak_text_buffer, "%.4f %s", ctx.true_peak_ch0, (ctx.true_peak_ch0 >= 1.0) ? "[CLIPPING!]" : "");
        ImGui::ProgressBar(static_cast<float>(ctx.true_peak_ch0), ImVec2(available_width, 16.0f), peak_text_buffer);
        ImGui::PopStyleColor();

        ImGui::Text("Ch 1 [R]:"); ImGui::SameLine(75.0f); 
        ImVec4 ch1_color = (ctx.true_peak_ch1 > 0.95) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ch1_color);
        sprintf(peak_text_buffer, "%.4f %s", ctx.true_peak_ch1, (ctx.true_peak_ch1 >= 1.0) ? "[CLIPPING!]" : "");
        ImGui::ProgressBar(static_cast<float>(ctx.true_peak_ch1), ImVec2(available_width, 16.0f), peak_text_buffer);
        ImGui::PopStyleColor();
        
        ImGui::Text("Đỉnh sóng tổng (Global True Peak): %.4f | Giới hạn an toàn: < 0.95", ctx.true_peak);

        ImGui::Spacing();
        ImGui::Separator();

        // --- 2. TÁI CẤU TRÚC: BIỂU ĐỒ ĐƯỜNG TUYẾN TÍNH LIÊN TỤC (LINEAR LINE & AREA CHART) ---
        const int WAVE_HISTORY_SIZE = 160; 
        static float history_current[WAVE_HISTORY_SIZE] = { -70.0f };
        static float history_avg[WAVE_HISTORY_SIZE] = { -70.0f };
        static int wave_offset = 0;

        // Lấy dữ liệu và tính toán Moving Average bằng bộ lọc thông thấp EMA (hệ số 0.05f)
        float current_loudness = static_cast<float>(ctx.loudness_momentary);
        static float running_avg = -70.0f;
        if (running_avg < -69.0f) running_avg = current_loudness; 
        running_avg = running_avg + 0.05f * (current_loudness - running_avg);

        // Nạp vào bộ đệm vòng (Ring Buffer)
        history_current[wave_offset] = current_loudness;
        history_avg[wave_offset] = running_avg;
        wave_offset = (wave_offset + 1) % WAVE_HISTORY_SIZE;

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.7f, 1.0f), "Đồ thị tuyến tính năng lượng (Đường nét: Tức thời | Vùng mờ: Trung bình tích lũy):");

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();           
        ImVec2 canvas_size = ImVec2(ImGui::GetContentRegionAvail().x, 90.0f); // Chiều cao nâng lên 90px để đồ thị tuyến tính biên độ rộng rõ ràng
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Vẽ Grid nền tối Studio
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(10, 12, 16, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(35, 40, 50, 255));

        // Vẽ các đường lưới ngang định chuẩn (Grid Lines) cho các mốc âm lượng quan trọng
        float lufs_markers[] = { -14.0f, -23.0f, -40.0f };
        for (float marker : lufs_markers) {
            float norm_m = (marker + 60.0f) / 60.0f;
            float y_m = canvas_pos.y + canvas_size.y - (norm_m * canvas_size.y * 0.90f) - (canvas_size.y * 0.05f);
            draw_list->AddLine(ImVec2(canvas_pos.x, y_m), ImVec2(canvas_pos.x + canvas_size.x, y_m), IM_COL32(50, 55, 65, 100), 1.0f);
        }

        float step_x = canvas_size.x / (WAVE_HISTORY_SIZE - 1);
        
        // Biến lưu tọa độ điểm trước đó để nối đường (Line Strip)
        ImVec2 prev_pt_cur, prev_pt_avg;

        for (int i = 0; i < WAVE_HISTORY_SIZE; i++) {
            int index = (wave_offset + i) % WAVE_HISTORY_SIZE;

            // 1. Chuẩn hóa giá trị Tức thời (Current)
            float lufs_cur = history_current[index];
            if (lufs_cur < -60.0f) lufs_cur = -60.0f;
            if (lufs_cur > 0.0f)  lufs_cur = 0.0f;
            float norm_cur = (lufs_cur + 60.0f) / 60.0f;

            // 2. Chuẩn hóa giá trị Trung bình (Average)
            float lufs_avg = history_avg[index];
            if (lufs_avg < -60.0f) lufs_avg = -60.0f;
            if (lufs_avg > 0.0f)  lufs_avg = 0.0f;
            float norm_avg = (lufs_avg + 60.0f) / 60.0f;

            // Tính toán tọa độ X và Y tuyến tính (Gốc tọa độ Y của ImGui tính từ đỉnh trên màn hình)
            // Chừa 5% padding trên dưới để đồ thị không bị chạm sát mép khung hình
            float x_pos = canvas_pos.x + (i * step_x);
            float graph_height_zone = canvas_size.y * 0.90f;
            float padding_y = canvas_size.y * 0.05f;

            float y_pos_cur = canvas_pos.y + canvas_size.y - (norm_cur * graph_height_zone) - padding_y;
            float y_pos_avg = canvas_pos.y + canvas_size.y - (norm_avg * graph_height_zone) - padding_y;
            float y_bottom  = canvas_pos.y + canvas_size.y;

            ImVec2 curr_pt_cur(x_pos, y_pos_cur);
            ImVec2 curr_pt_avg(x_pos, y_pos_avg);

            if (i > 0) {
                // ==========================================
                // LỚP 1: VẼ VÙNG ĐỔ BÓNG TRUNG BÌNH (AVERAGE AREA CHART)
                // ==========================================
                // Tạo một hình đa giác nhỏ nối từ đáy lên điểm cũ, qua điểm mới rồi hạ xuống đáy
                ImVec2 points[4] = {
                    ImVec2(prev_pt_avg.x, y_bottom - 1.0f),
                    prev_pt_avg,
                    curr_pt_avg,
                    ImVec2(curr_pt_avg.x, y_bottom - 1.0f)
                };
                
                ImU32 area_color = IM_COL32(0, 100, 160, 45); // Xanh dương mờ ảo làm nền
                if (history_avg[index] > -14.0f) {
                    area_color = IM_COL32(180, 80, 0, 45);   // Chuyển sang cam mờ nếu năng lượng trung bình cao
                }
                draw_list->AddConvexPolyFilled(points, 4, area_color);

                // Vẽ thêm một đường chỉ mảnh biên giới cho đường Trung bình mượt mà
                draw_list->AddLine(prev_pt_avg, curr_pt_avg, IM_COL32(0, 140, 200, 120), 1.5f);

                // ==========================================
                // LỚP 2: VẼ ĐƯỜNG TUYẾN TÍNH TỨC THỜI (CURRENT LINE GRAPH)
                // ==========================================
                ImU32 line_color;
                if (history_current[index] > -5.0f) {
                    line_color = IM_COL32(255, 30, 30, 255);   // Đỏ rực chói tai
                } else if (history_current[index] > -14.0f) {
                    line_color = IM_COL32(255, 150, 0, 255);  // Cam Modern Target
                } else {
                    line_color = IM_COL32(0, 255, 180, 255);  // Đường Cyan Neon mảnh mai, sắc nét
                }

                // Vẽ đường nối liền mạch giữa các frame dữ liệu
                draw_list->AddLine(prev_pt_cur, curr_pt_cur, line_color, 2.0f);
            }

            // Lưu lại điểm hiện tại làm điểm lùi cho frame kế tiếp
            prev_pt_cur = curr_pt_cur;
            prev_pt_avg = curr_pt_avg;
        }

        ImGui::Dummy(canvas_size); 
        ImGui::Spacing();
        ImGui::Separator();

        // =========================================================================
        // 4. TRẠM GIÁM SÁT BIÊN ĐỘ DAO ĐỘNG CỰC ĐẠI (LOUDNESS OSCILLATION MATRIX)
        // =========================================================================
        ImGui::TextColored(ImVec4(255.0f/255.0f, 150.0f/255.0f, 0.0f, 1.0f), "4. Trạm giám sát biên độ dao động cực đại (Loudness Oscillation Matrix):");

        // --- [NÂNG CẤP]: BIẾN PHÂN LY ĐỘ NHẠY ĐỒ HỌA ---
        // Cậu có thể chuyển biến này thành biến thành viên (member variable) của Class nếu muốn lưu cấu hình
        static float m_oscillationSensitivity = 1.0f; 
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat("Độ nhạy phân ly", &m_oscillationSensitivity, 0.5f, 2.5f, "%.2fx");
        ImGui::SameLine(); ImGui::TextDisabled("(Tăng khi nhạc bị nén phẳng, giảm khi nhạc giật quá mạnh)");

        // --- TRÍCH XUẤT ĐỘ LỚN DAO ĐỘNG THỰC TẾ ---
        float loudness_delta = std::abs(static_cast<float>(ctx.loudness_momentary) - static_cast<float>(ctx.loudness_shortterm));
        
        // Khử nhiễu toán học khi bài hát rơi vào khoảng lặng
        if (ctx.loudness_momentary < -55.0f || ctx.loudness_shortterm < -55.0f) {
            loudness_delta = 0.0f;
        }

        // CHUẨN HÓA ĐỘ ĐỘNG THÍCH ỨNG (Áp dụng hệ số phân ly độ nhạy)
        // Nhân trực tiếp với m_oscillationSensitivity để khuếch đại hoặc thu nhỏ độ lệch Delta trước khi map vào dải [0.0f, 1.0f]
        float scaled_delta = loudness_delta * m_oscillationSensitivity;
        float raw_oscillation = std::min(1.0f, scaled_delta / 12.0f);

        // --- BỘ LỌC VẬT LÝ QUÁN TÍNH ĐỘNG NĂNG (ATTACK & DECAY PHYSICS ENGINE) ---
        static float oscillation_smooth = 0.0f;
        float dt = ImGui::GetIO().DeltaTime;
        
        if (raw_oscillation > oscillation_smooth) {
            oscillation_smooth = raw_oscillation; // Fast Attack
        } else {
            // Heavy Decay (Hệ số phục hồi 4.0f)
            oscillation_smooth -= 4.0f * dt * (oscillation_smooth - raw_oscillation);
            if (oscillation_smooth < 0.0f) oscillation_smooth = 0.0f;
        }

        // --- CẤU HÌNH KHÔNG GIAN VẼ BIỂU ĐỒ ---
        ImVec2 b_canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 b_canvas_size = ImVec2(ImGui::GetContentRegionAvail().x, 110.0f); 
        ImDrawList* b_draw_list = ImGui::GetWindowDrawList();

        // Vẽ nền tối Studio đặc trưng
        b_draw_list->AddRectFilled(b_canvas_pos, ImVec2(b_canvas_pos.x + b_canvas_size.x, b_canvas_pos.y + b_canvas_size.y), IM_COL32(8, 10, 14, 255));
        b_draw_list->AddRect(b_canvas_pos, ImVec2(b_canvas_pos.x + b_canvas_size.x, b_canvas_pos.y + b_canvas_size.y), IM_COL32(60, 45, 20, 255)); 

        // Xác định tâm hình học để dựng Lõi Động Năng
        ImVec2 center_node = ImVec2(b_canvas_pos.x + b_canvas_size.x / 2.0f, b_canvas_pos.y + b_canvas_size.y / 2.0f);
        
        // =========================================================================
        // LỚP 1: LÕI NĂNG LƯỢNG DAO ĐỘNG TRUNG TÂM (CENTRAL PULSING KINETIC CORE)
        // =========================================================================
        float base_radius = 18.0f;      
        float max_expansion = 20.0f;    
        float current_radius = base_radius + (oscillation_smooth * max_expansion);

        // Phát quang tỏa rạng (Kinetic Glow Aura)
        int aura_layers = 3;
        for (int i = 1; i <= aura_layers; i++) {
            float aura_radius = current_radius + (i * 5.0f * oscillation_smooth);
            int alpha = static_cast<int>((50 / i) * oscillation_smooth);
            b_draw_list->AddCircleFilled(center_node, aura_radius, IM_COL32(240, 120, 0, alpha), 32);
        }

        // Lõi năng lượng chính - Màu Cam Hổ Phách
        ImU32 core_color = IM_COL32(220, 100, 0, 255);
        if (oscillation_smooth > 0.80f) core_color = IM_COL32(255, 40, 0, 255); 
        
        b_draw_list->AddCircleFilled(center_node, current_radius, core_color, 36);
        b_draw_list->AddCircle(center_node, current_radius, IM_COL32(255, 230, 180, 200), 36, 1.5f); 

        // =========================================================================
        // LỚP 2: ĐỒ THỊ SÓNG DAO ĐỘNG ĐỐI XỨNG LAN TỎA (OSCILLATION WINGS)
        // =========================================================================
        const int OSCILLATION_HISTORY = 45;
        static float oscillation_history_array[OSCILLATION_HISTORY] = { 0.0f };
        static int oscillation_history_offset = 0;
        
        oscillation_history_array[oscillation_history_offset] = oscillation_smooth;
        oscillation_history_offset = (oscillation_history_offset + 1) % OSCILLATION_HISTORY;

        float wing_width = (b_canvas_size.x / 2.0f) - current_radius - 15.0f;
        float bar_step = wing_width / OSCILLATION_HISTORY;

        for (int i = 0; i < OSCILLATION_HISTORY; i++) {
            int index = (oscillation_history_offset - 1 - i + OSCILLATION_HISTORY) % OSCILLATION_HISTORY;
            float hist_val = oscillation_history_array[index];

            float bar_h = hist_val * (b_canvas_size.y * 0.42f);
            if (bar_h < 1.0f) bar_h = 1.0f; 

            int wave_alpha = static_cast<int>(std::max(15.0f, 255.0f * (1.0f - (static_cast<float>(i) / OSCILLATION_HISTORY))));
            ImU32 wing_color = IM_COL32(230, 140, 10, wave_alpha); 
            if (hist_val > 0.80f) wing_color = IM_COL32(255, 60, 40, wave_alpha); 

            // Cánh phải (Right Wing)
            float rx = center_node.x + current_radius + 10.0f + (i * bar_step);
            b_draw_list->AddRectFilled(ImVec2(rx, center_node.y - bar_h), ImVec2(rx + bar_step - 1.0f, center_node.y + bar_h), wing_color, 1.0f);

            // Cánh trái đối xứng (Left Wing)
            float lx = center_node.x - current_radius - 10.0f - (i * bar_step) - bar_step;
            b_draw_list->AddRectFilled(ImVec2(lx, center_node.y - bar_h), ImVec2(lx + bar_step - 1.0f, center_node.y + bar_h), wing_color, 1.0f);
        }


        ImGui::Dummy(b_canvas_size);
        ImGui::Spacing();
        ImGui::Separator();

        // --- KHỐI MA TRẬN PHÂN TÍCH DẢI ĐỘNG (LRA ADAPTIVE ANALYSIS) ---
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "Chỉ số tích lũy & Phân đoạn dải động (Loudness Range Matrix):");
        
        ImGui::Columns(3, "LraGrid", false);
        ImGui::Text("Độ to Tích lũy (Integrated):");
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "  %.1f LUFS", ctx.loudness_integrated);
        ImGui::NextColumn();
        
        ImGui::Text("Dải Động (LRA):");
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "  %.1f LU (Loudness Units)", ctx.loudness_range);
        ImGui::NextColumn();

        ImGui::Text("Cửa sổ Năng lượng (Low/High):");
        ImGui::TextDisabled("  Low: %.1f | High: %.1f", ctx.loudness_lra_low, ctx.loudness_lra_high);
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();

    }
}



void DrawLogTab(AudioFilterManager& manager)
{
    g_PopupLogger.Draw();

    ImGui::Spacing();

    if (ImGui::Button("Clear Logs"))
    {
        g_PopupLogger.Clear();
    }
}


void ShowTestPopup(bool& closePopup_Test, WindowRuntime* window)
{
    if (!window) return;
    auto* session = window->resource.GetPlayerSession();
    if (!session) return;
    auto* manager = session->GetAudioFilterManager();
    if(!manager) return;

    AudioContext ctx = manager->GetCurrentContext();

    if (CSImGui::BeginModernTabBar("##DebugTabs"))
    {
        if (CSImGui::ModernTabItem("Core"))
        {
            DrawCoreMonitor(*manager, ctx);
            CSImGui::EndModernTabItem();
        }

        if (CSImGui::ModernTabItem("Adaptive"))
        {
            DrawAdaptiveControl(*manager, ctx);
            CSImGui::EndModernTabItem();
        }

        if (CSImGui::ModernTabItem("EBU R128"))
        {
            DrawEBUR128Monitor(*manager, ctx);
            CSImGui::EndModernTabItem();
        }

        if (CSImGui::ModernTabItem("Visualizer"))
        {
            DrawAudioVisualizer(*session);
            CSImGui::EndModernTabItem();
        }

        if (CSImGui::ModernTabItem("Logs"))
        {
            DrawLogTab(*manager);
            CSImGui::EndModernTabItem();
        }

        CSImGui::EndModernTabBar();
    }

    ImGui::Separator();

    if (ImGui::Button("Thoát"))
        closePopup_Test = true;
}

void OpenTestPopup(ReusablePopup& popup) {
    popup.Open("Audio Filter & Advanced Diagnostics Engine Dashboard", [](WindowRuntime* runtime, bool& closePopup_Test) {
        ShowTestPopup(closePopup_Test, runtime);  
    });
}

void RenderTestPopup(ReusablePopup& popup, WindowRuntime* window){
    if(popup.IsOpen()) {
        popup.Render(window);
    }
}