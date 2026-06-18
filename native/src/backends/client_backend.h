#pragma once

#include "globals.h"
#include "utils.h"

#include <unordered_map>
#include <string>
#include <vector>

enum class VideoSource {
    Search,
    Trending,
    Watched,
    AllKeys,
    Keywords,
    Batch
};

struct VideoItem {
    std::string id;
    std::string title;
    std::string thumbnail_url;
    std::string link;
    std::string channel;   
    std::string duration;  
    std::string uploadDate; 

    std::string views;
    std::string likes;

    bool isLive =false;
    bool isShorts =false;

    bool thumb_loaded ;
    
    float hoverAnim = 0.0f;
    float titleHoverTime = 0.0f;
};

struct ThumbnailCache {
    GLuint tex;
    int w, h;
    bool loaded;
};
struct ThumbnailRequest {
    std::string id;
    int w,h;
    std::vector<unsigned char> pixels;
};

extern std::vector<VideoItem> g_videoList;
extern std::unordered_map<std::string, ThumbnailCache> g_thumbnailCache;
extern std::vector<ThumbnailRequest> g_thumbnailQueue;
extern std::atomic<bool> g_loading;


void UpdateVideoData(VideoSource source, const std::string& param = "", int top = 20);
void ProcessThumbnailQueue();
void LoadThumbnail(const VideoItem &v);
void TrimThumbnailCache();

