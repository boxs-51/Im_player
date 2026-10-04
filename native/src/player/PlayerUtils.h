#pragma once
#include <mpv/client.h>
#include "player/PlayerStateSystem.h"
#include <iostream>
#include <log.h>

static void SetMPVOptions(mpv_handle* mpv, const std::unordered_map<std::string, std::string>& options, bool isProperty = false) {
    for (const auto& [key, value] : options) {
        if (isProperty) {
            int err = mpv_set_property_string(mpv, key.c_str(), value.c_str());
            if (err < 0) {
                LOG(0, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_PROPERTY_ERROR: Key '%s', Value '%s', Error code: %s", key.c_str(), value.c_str(), mpv_error_string(err));
            } else {
                LOG(0, LogLevel::Info, LogCategory::System, "[SUCCESS] Set MPV Property: Key '%s' = '%s'", key.c_str(), value.c_str());
            }
        } else {
            int err = mpv_set_option_string(mpv, key.c_str(), value.c_str());
            if (err < 0) {
                LOG(0, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_OPTION_ERROR: Key '%s', Value '%s', Error code: %s", key.c_str(), value.c_str(), mpv_error_string(err));
            } else {
                LOG(0, LogLevel::Info, LogCategory::System, "[SUCCESS] Set MPV Option: Key '%s' = '%s'", key.c_str(), value.c_str());
            }
        }
    }
}

inline void ApplyStaticMPVConfig(mpv_handle* mpv) {
    if (!mpv) return;
    
    SetMPVOptions(mpv, {
        {"idle", "yes"},
        {"keep-open", "yes"},
        {"stop-screensaver", "yes"},
        {"vo", "libmpv"},
        {"hwdec", "auto-safe"}, // Tự động chọn giải mã phần cứng ổn định nhất
        //{"video-rotate", "no"},
        //{"tls-verify", "no"},   // Hữu ích cho một số link stream https không chuẩn
        
        // Cấu hình âm thanh an toàn
        {"audio-buffer", "0.5"}, // Đơn vị giây, 0.2s là đủ mượt và không gây trễ
        //{"audio-pitch-correction", "yes"}, // Giữ tone giọng khi thay đổi speed

        {"cookies", "yes"},
        //{"ytdl-raw-options", "user-agent=Mozilla/5.0,referer=https://www.youtube.com/"},

    });
    
}
inline void ApplyDynamicMPVConfig(mpv_handle* mpv, VideoType type) {
    if (!mpv) return;

    std::unordered_map<std::string, std::string> config;
    switch (type) {
        case VideoType::Live:
            config = {
                {"profile", "low-latency"},
                {"untimed", "yes"},
                {"cache", "yes"},
                {"cache-pause", "no"},         // Quan trọng: Không dừng hình khi mất kết nối tạm thời
                {"cache-secs", "5"},           // Giảm xuống 5s để giảm độ trễ thực tế
                {"demuxer-max-bytes", "50M"},
                {"demuxer-max-back-bytes", "0"},
                {"vd-lavc-fast", "yes"},       // Giải mã nhanh
                {"video-sync", "audio"},       // Khớp hình theo tiếng
                {"stream-buffer-size", "512k"},
            };
            break;
        case VideoType::Local:
            config = {
                {"cache", "auto"},
                {"cache-pause", "yes"},
                {"demuxer-max-bytes", "500M"},     // Tăng lên để đọc file 4K mượt hơn
                {"demuxer-max-back-bytes", "100M"}, // Cho phép tua ngược trong RAM mượt mà
                {"video-sync", "display-resample"},// Mượt hình trên màn hình (Sync theo tần số quét)
                {"interpolation", "yes"},          // Chống giật hình (judder)
                {"tscale", "oversample"},          // Đi kèm với interpolation
                
                // Bỏ ewa_lanczossharp nếu máy yếu, vì nó tốn tài nguyên GPU
                {"scale", "bilinear"},             // Mặc định an toàn, đổi thành lanczos nếu GPU mạnh
                {"cscale", "bilinear"},
            };
            break;
        case VideoType::Vio:
            config = {
                {"cache", "yes"},
                {"cache-pause", "yes"},
                // Tăng thời gian lưu trữ trong cache lên 5 phút (300 giây)
                {"cache-secs", "300"}, 
                // Quan trọng: Tăng giới hạn dung lượng RAM để chứa đủ 5 phút video đó
                // 1GB (1024M) là thoải mái cho video 4K bitrate cao
                {"demuxer-max-bytes", "1024M"}, 
                // Cho phép bộ giải mã đọc trước 300 giây
                {"demuxer-readahead-secs", "300"},
                // Cho phép lưu lại phần đã xem để tua ngược nhanh (nếu muốn)
                {"demuxer-max-back-bytes", "200M"},
                {"video-sync", "audio"},
                {"network-timeout", "60"}, // Tăng timeout lên 60s cho mạng cực chậm
            };
            break;
        default:
        {
            config = {
                {"cache", "auto"},
                {"cache-pause", "yes"},
                {"cache-secs", "10"},
                {"hr-seek", "yes"},
                {"hr-seek-framedrop", "yes"},
                {"initial-audio-sync", "no"},
                {"vd-lavc-skipframe", "default"},
                // Framedrop nên để mặc định là yes để tránh lệch tiếng khi máy lag
                {"framedrop", "vo"},
                // Kích hoạt tính năng deband
                {"deband", "yes"},
                // Tinh chỉnh thông số (tùy chọn để đạt chất lượng tốt hơn)
                {"deband-iterations", "2"}, // Số lần lặp, mặc định là 1
                {"deband-threshold", "48"}, // Độ nhạy (mặc định 32)
                {"deband-range", "16"},     // Phạm vi lấy mẫu (mặc định 16)
                {"deband-grain", "24"},     // Lượng nhiễu hạt để che vết rổ
            };
            break;
        }
    };

    SetMPVOptions(mpv, config, true);
}

inline static void PrintMPVNode(mpv_handle* mpv, const mpv_node* node, int indent = 0) {
    if (!mpv) return;

    mpv_node result;
    if (mpv_get_property(mpv, "config", MPV_FORMAT_NODE, &result) >= 0) {
        std::cout << "[DEBUG] Dump config:\n";
        PrintMPVNode(mpv, &result);
        std::cout << std::endl;
        mpv_free_node_contents(&result);
    }
    
    if (!node) return;

    std::string pad(indent, ' ');

    switch (node->format) {
    case MPV_FORMAT_STRING:
        std::cout << pad << "\"" << node->u.string << "\"";
        break;
    case MPV_FORMAT_INT64:
        std::cout << pad << node->u.int64;
        break;
    case MPV_FORMAT_DOUBLE:
        std::cout << pad << node->u.double_;
        break;
    case MPV_FORMAT_FLAG:
        std::cout << pad << (node->u.flag ? "true" : "false");
        break;
    case MPV_FORMAT_NONE:
        std::cout << pad << "null";
        break;

    case MPV_FORMAT_NODE_ARRAY: {
        std::cout << pad << "[\n";
        for (int i = 0; i < node->u.list->num; ++i) {
            PrintMPVNode(mpv, &node->u.list->values[i], indent + 2);
            if (i < node->u.list->num - 1) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << pad << "]";
        break;
    }

    case MPV_FORMAT_NODE_MAP: {
        std::cout << pad << "{\n";
        for (int i = 0; i < node->u.list->num; ++i) {
            std::cout << std::string(indent + 2, ' ')
                      << "\"" << node->u.list->keys[i] << "\": ";
            PrintMPVNode(mpv, &node->u.list->values[i], indent + 2);
            if (i < node->u.list->num - 1) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << pad << "}";
        break;
    }

    default:
        std::cout << pad << "<unknown format " << node->format << ">";
        break;
    }
}

static void HandlePropertyChange(mpv_handle* mpv, mpv_event* event) {
    if (!mpv ||!event || event->event_id != MPV_EVENT_PROPERTY_CHANGE) return;
    mpv_event_property* prop = (mpv_event_property*)event->data;
    if (!prop || !prop->name) return;

    std::cout << "{\n";
    std::cout << "  \"property\": \"" << prop->name << "\",\n";
    std::cout << "  \"value\": ";

    if (prop->format == MPV_FORMAT_NODE && prop->data) {
        PrintMPVNode(mpv, (mpv_node*)prop->data, 2);
    } else if (prop->format == MPV_FORMAT_STRING) {
        std::cout << "\"" << *(const char**)prop->data << "\"";
    } else if (prop->format == MPV_FORMAT_INT64) {
        std::cout << *(int64_t*)prop->data;
    } else if (prop->format == MPV_FORMAT_DOUBLE) {
        std::cout << *(double*)prop->data;
    } else if (prop->format == MPV_FORMAT_FLAG) {
        std::cout << (*(bool*)prop->data ? "true" : "false");
    } else {
        std::cout << "null";
    }

    std::cout << "\n}\n";
}


inline const mpv_node* mpv_node_dict_find(const mpv_node *node, const char *key) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP)
        return nullptr;

    const mpv_node_list *list = node->u.list;
    for (int i = 0; i < list->num; i++) {
        if (strcmp(list->keys[i], key) == 0) {
            return &list->values[i];
        }
    }
    return nullptr;
}

inline const char* PlaybackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::Idle:        return "Idle";
        case PlaybackState::Loading:     return "Loading";
        case PlaybackState::Playing:     return "Playing";
        case PlaybackState::Paused:      return "Paused";
        case PlaybackState::Seeking:     return "Seeking";
        case PlaybackState::EndOfFile:   return "End of File";
        default:                         return "Unknown";
    }
}

inline const char* VideoTypeToString(VideoType type) {
    switch (type) {
        case VideoType::None:          return "None";
        case VideoType::Vio:           return "Vio Video";
        case VideoType::Live:          return "Live Stream";
        case VideoType::Local:         return "Local File";
        default:                        return "Unknown";
    }
}
