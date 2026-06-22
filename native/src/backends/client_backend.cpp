#include "utils.h"
#include "json.hpp"
#include "cpr.h"
#include "client_backend.h"
#include "stb_image.h"

#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

std::vector<ThumbnailRequest> g_thumbnailQueue;
std::unordered_map<std::string, ThumbnailCache> g_thumbnailCache;
std::vector<VideoItem> g_videoList;
std::unordered_set<std::string> g_videoIds; // Bộ lọc ID tốc độ cao
const size_t MAX_VIDEO_LIST_SIZE = 100;    // Giới hạn danh sách

std::atomic<bool> g_loading{false};
std::unordered_set<std::string> g_loadedIds;
const std::string BACKEND_HOST = "http://127.0.0.1:8000";

auto parseDuration = [](const std::string &iso) -> std::string {
    int h = 0, m = 0, s = 0;
    std::string temp;
    for (size_t i = 0; i < iso.size(); ++i) {
        if (isdigit(iso[i])) temp += iso[i];
        else if (iso[i] == 'H') { h = std::stoi(temp); temp.clear(); }
        else if (iso[i] == 'M') { m = std::stoi(temp); temp.clear(); }
        else if (iso[i] == 'S') { s = std::stoi(temp); temp.clear(); }
    }
    char buf[16];
    if (h > 0)
        sprintf(buf, "%d:%02d:%02d", h, m, s);
    else
        sprintf(buf, "%02d:%02d", m, s);
    return buf;
};

auto parseDate = [](const std::string &iso) -> std::string {
    // Dạng: "2025-10-07T08:12:00Z"
    if (iso.size() < 10) return iso;
    int y, M, d;
    sscanf(iso.substr(0, 10).c_str(), "%d-%d-%d", &y, &M, &d);

    // Hiển thị ngắn gọn DD/MM/YYYY
    char buf[16];
    sprintf(buf, "%02d/%02d/%04d", d, M, y);
    return buf;
};
/// --- Hàm lấy dữ liệu video từ backend Python ---
void UpdateVideoData(VideoSource source, const std::string& param, int top)
{
    if (g_loading.exchange(true)) return;

    std::thread([source, param, top]() {
        try {
            std::string url;
            cpr::Response r;

            // --- Build URL/API ---
            switch (source) {
                case VideoSource::Search:
                    if (!g_searchQuery.empty()) {
                        url = BACKEND_HOST + "/search?q=" +
                              std::string(cpr::util::urlEncode(g_searchQuery));
                        if (!g_nextPageToken.empty())
                            url += "&pageToken=" + g_nextPageToken;
                        r = cpr::Get(cpr::Url{url});
                    }
                    break;

                case VideoSource::Trending:
                    url = BACKEND_HOST + "/trending";
                    if (!g_nextPageToken.empty())
                        url += "?pageToken=" + g_nextPageToken;
                    r = cpr::Get(cpr::Url{url});
                    break;

                case VideoSource::Watched:
                    url = BACKEND_HOST + "/watched?url=" +
                          std::string(cpr::util::urlEncode(param));
                    r = cpr::Get(cpr::Url{url});
                    break;

                case VideoSource::AllKeys:
                    url = BACKEND_HOST + "/all-keys";
                    r = cpr::Get(cpr::Url{url});
                    break;

                case VideoSource::Keywords:
                    url = BACKEND_HOST + "/keywords?top=" + std::to_string(top);
                    r = cpr::Get(cpr::Url{url});
                    break;

                case VideoSource::Batch:
                    r = cpr::Post(
                        cpr::Url{BACKEND_HOST + "/batch-videos"},
                        cpr::Body{param},
                        cpr::Header{{"Content-Type", "application/json"}}
                    );
                    break;
            }

            // --- Parse JSON ---
            if (r.status_code == 200) {
                auto json = nlohmann::json::parse(r.text);

                std::lock_guard<std::mutex> lock(g_mutex);

                // Nếu có videos
                if (json.contains("videos") && json["videos"].is_array()) {
                    for (auto &v : json["videos"]) {

                        if (g_videoIds.count(v)) continue;

                        // 2. Nếu danh sách quá dài, xóa video cũ nhất (đầu danh sách)
                        if (g_videoList.size() >= MAX_VIDEO_LIST_SIZE) {
                            // Lấy video cũ nhất
                            const std::string& oldId = g_videoList.front().id;
                            
                            // Xóa ID khỏi Set lọc trùng
                            g_videoIds.erase(oldId);
                            
                            // GIẢI PHÓNG TEXTURE: Rất quan trọng để tránh tràn bộ nhớ GPU
                            if (g_thumbnailCache.count(oldId)) {
                                if (g_thumbnailCache[oldId].tex != 0) {
                                    glDeleteTextures(1, &g_thumbnailCache[oldId].tex);
                                }
                                g_thumbnailCache.erase(oldId);
                            }

                            // Xóa khỏi danh sách hiển thị
                            g_videoList.erase(g_videoList.begin());
                        }
                        
                        VideoItem item;
                        item.id = v.value("id", "");
                        item.title = v.value("title", "");
                        item.channel = v.value("channel", "");
                        item.duration = parseDuration(v.value("duration", ""));
                        item.uploadDate = parseDate(v.value("publishedAt", ""));
                        item.thumbnail_url = v.value("thumbnail", "");
                        item.link = v.value("link", "");

                        item.views= v.value("views", "0");
                        item.likes= v.value("likes", "0");

                        //std::string liveStr = v.value("isLive", "none");
                        //item.isLive = (liveStr == "live");

                        //std::string shortsStr = v.value("isShorts", "false");
                        //item.isShorts = (shortsStr == "true" || shortsStr == "1");

                        item.thumb_loaded = false;
                        g_loadedIds.insert(v["id"].get<std::string>());
                        g_videoList.push_back(item);
                    }

                    if (json.contains("nextPageToken"))
                        g_nextPageToken = json["nextPageToken"].get<std::string>();
                    else
                        g_nextPageToken.clear();
                }

                // Nếu có keywords
                if (json.contains("top_keywords") && json["top_keywords"].is_array()) {
                    g_keywords.clear();
                    for (auto &item : json["top_keywords"]) {
                        if (item.is_string()) {
                            g_keywords.push_back(item.get<std::string>());
                        } else if (item.is_array() && item.size() > 0 && item[0].is_string()) {
                            g_keywords.push_back(item[0].get<std::string>());
                        }
                    }
                }
            } else {
                std::cerr << "HTTP error: " << r.status_code << std::endl;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error fetching videos: " << e.what() << std::endl;
        }

        g_loading = false;
    }).detach();
}

constexpr size_t MAX_THUMB_CACHE = 50;

static void ReleaseTexture(GLuint tex)
{
    if (tex != 0)
        glDeleteTextures(1, &tex);
}

void TrimThumbnailCache()
{
    while (g_thumbnailCache.size() > MAX_THUMB_CACHE)
    {
        auto it = g_thumbnailCache.begin();

        // ❗ Giải phóng GPU texture
        if (it->second.tex)
            ReleaseTexture(it->second.tex);

        g_thumbnailCache.erase(it);
    }
}

void LoadThumbnail(const VideoItem &v)
{
    if (g_thumbnailCache.count(v.id)) return;

    g_thumbnailCache[v.id] = ThumbnailCache{0,0,false};

    std::thread([v]() {
        try {
            std::string url = BACKEND_HOST + "/thumbnail?url=" +
                               std::string(cpr::util::urlEncode(v.thumbnail_url));
            auto r = cpr::Get(cpr::Url{url});
            if (r.status_code == 200) {
                int w,h,n;
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<const unsigned char*>(r.text.c_str()),
                    r.text.size(),
                    &w,&h,&n,4
                );
                if (data) {
                    std::vector<unsigned char> pixels(data, data + (w*h*4));
                    stbi_image_free(data);

                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_thumbnailQueue.push_back({v.id, w, h, std::move(pixels)});
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Load thumbnail error: " << e.what() << std::endl;
        }
    }).detach();
}
void ProcessThumbnailQueue()
{
    std::vector<ThumbnailRequest> local;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        local.swap(g_thumbnailQueue);
    }

    for (auto &req : local)
    {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            req.w,
            req.h,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            req.pixels.data()
        );
        glBindTexture(GL_TEXTURE_2D, 0);

        g_thumbnailCache[req.id] = ThumbnailCache{tex, req.w, req.h, true};
    }
}