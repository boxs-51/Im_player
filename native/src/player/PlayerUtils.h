#pragma once
#include <mpv/client.h>
#include <iostream>


void PrintMPVNode(mpv_handle* mpv, const mpv_node* node, int indent = 0) {
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


const mpv_node* mpv_node_dict_find(const mpv_node *node, const char *key) {
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

const char* PlaybackStateToString(PlaybackState state) {
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

const char* VideoTypeToString(VideoType type) {
    switch (type) {
        case VideoType::None:          return "None";
        case VideoType::Vio:           return "Vio Video";
        case VideoType::Live:          return "Live Stream";
        case VideoType::Local:         return "Local File";
        default:                        return "Unknown";
    }
}
