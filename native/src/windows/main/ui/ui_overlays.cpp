#include "ui_overlays.h"

#include "gui/gui.h"
#include "utils.h"
#include <cmath>
#include "windows/WindowRuntime.h"

#include <GL/gl3w.h> 


GLuint GetIcon(const std::string &name);

void RenderIdleBackground(const std::string& imagePath, const ImVec2& _pos, const ImVec2& _size) {
    static ImTextureID cachedImTexID = (ImTextureID)0; 
    static std::string cachedPath = "";

    if (!imagePath.empty() && imagePath != cachedPath) {
        GLuint tex = GetIcon(imagePath);
        
        if (tex != 0) {
            cachedImTexID = (ImTextureID)(intptr_t)tex;
            cachedPath = imagePath;
        }
    }

    if (cachedImTexID != (ImTextureID)0) {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        const ImVec2 p_min = _pos;
        const ImVec2 p_max = _pos + _size;

        draw_list->AddImage(cachedImTexID, p_min, p_max);
    }
}

void CleanupIcons() {}

void RenderLoading(const ImVec2& _pos, const ImVec2& _size) {
    static LoadingIconData centralLoading;
    float idealSize = _size.x * 0.10f; 
    
    float minSize = 35.0f;
    float maxSize = 110.0f;
    float finalSize = ImClamp(idealSize, minSize, maxSize);

    ImVec2 Pos = _pos;
    ImVec2 Size = _size;

    centralLoading.pos = ImVec2(
        Pos.x + (Size.x * 0.5f) - (finalSize * 0.5f), 
        Pos.y + (Size.y * 0.5f) - (finalSize * 0.5f)
    );
    centralLoading.size = ImVec2(finalSize, finalSize);

    DrawLoadingIconAnimated(
        ImGui::GetWindowDrawList(), 
        ImVec2(0,0), ImVec2(0,0),
        IM_COL32(255, 255, 255, 255), 
        &centralLoading
    );
}

void RenderSeekingOverlay(WindowRuntime* runtime, const ImVec2& _pos, const ImVec2& _size) {
    
    auto* player_session = runtime->resource.GetPlayerSession();
    auto* player_state = player_session->GetState();
    if (!player_state) return;

    static SeekingData data; 
  
    player_state->ReadPlayback([&](PlaybackModel const& m){
        data.g_isSeeking = m.flags.isSeeking;
        data.currentTime = m.timing.timePos;
    });

    float dt = ImGui::GetIO().DeltaTime;

    if (data.g_isSeeking && !data.lastSeekingState) {
        data.pulse = 1.0f;
    }
    data.lastSeekingState = data.g_isSeeking;

    if (data.g_isSeeking && data.currentTime != data.lastTime) {
        data.forward = (data.currentTime > data.lastTime);
    }
    data.lastTime = data.currentTime;

    float targetAlpha = data.g_isSeeking ? 1.0f : 0.0f;
    float lerpSpeed = data.g_isSeeking ? 10.0f : 3.0f;
    data.alpha = ImLerp(data.alpha, targetAlpha, ImMin(dt * lerpSpeed, 1.0f));

    data.pulse = ImLerp(data.pulse, 0.0f, ImMin(dt * 6.0f, 1.0f));

    if (data.alpha > 0.001f) {
        data.timer += dt * 2.5f; 
        if (data.timer > 1.0f) data.timer -= 1.0f; 
        
        data.pos = _pos;
        data.size = _size;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 color = IM_COL32(255, 255, 255, 255);
        
        ImVec2 center;
        float fullW = data.size.x;
        float fullH = data.size.y;

        if (data.forward) {
            center = ImVec2(data.pos.x + fullW * 0.75f, data.pos.y + fullH * 0.5f);
        } else {
            center = ImVec2(data.pos.x + fullW * 0.25f, data.pos.y + fullH * 0.5f);
        }

        float dir = data.forward ? 1.0f : -1.0f;
        
        float scale = 1.0f + (data.pulse * 0.2f);
        float triW = fullW * 0.05f * scale; 
        float triH = fullH * 0.20f * scale;
        float spacing = triW * 1.0f;

        float speed = 1.5f;
        float t = fmodf(data.timer * speed, 1.0f);

        ImVec4 colVec = ImGui::ColorConvertU32ToFloat4(color);
        ImVec4 brightCol = ImVec4(
            ImMin(colVec.x + 0.3f, 1.0f), 
            ImMin(colVec.y + 0.3f, 1.0f), 
            ImMin(colVec.z + 0.3f, 1.0f), 
            colVec.w * data.alpha
        );
        float baseAlpha = brightCol.w;

        ImU32 bgGlowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, baseAlpha * 0.15f));
        ImU32 transparent = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, 0.0f));

        if (data.forward) {
            drawList->AddRectFilledMultiColor(
                ImVec2(data.pos.x + fullW * 0.5f, data.pos.y), 
                ImVec2(data.pos.x + fullW, data.pos.y + fullH),
                transparent, bgGlowCol, bgGlowCol, transparent
            );
        } else {
            drawList->AddRectFilledMultiColor(
                ImVec2(data.pos.x, data.pos.y), 
                ImVec2(data.pos.x + fullW * 0.5f, data.pos.y + fullH),
                bgGlowCol, transparent, transparent, bgGlowCol
            );
        }

        int glowLayers = 6; 
        float maxGlowRadius = triH * 0.7f;

        for (int i = 0; i < 3; i++) {
            float offset = ((float)i + t) * spacing;
            float x = center.x + (offset - (spacing * 1.5f)) * dir;
            ImVec2 glowPos = ImVec2(x + (dir > 0 ? triW * 0.4f : -triW * 0.4f), center.y);

            for (int layer = 1; layer <= glowLayers; layer++) {
                float fraction = (float)layer / (float)glowLayers;
                float r = maxGlowRadius * fraction;
                float lAlpha = (1.0f - fraction) * 0.3f * baseAlpha; 
                
                if (lAlpha <= 0.0f) continue;
                
                ImU32 gCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, lAlpha));
                drawList->AddCircleFilled(glowPos, r, gCol, 16);
            }
        }

        for (int i = 0; i < 3; i++) {
            float offset = ((float)i + t) * spacing;
            float x = center.x + (offset - (spacing * 1.5f)) * dir;

            float progress = (float)(i + 1) / 3.0f;
            float triangleAlpha = baseAlpha * progress * (1.0f - t * 0.2f);
            
            ImU32 finalCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, triangleAlpha));

            ImVec2 p1, p2, p3;
            float tipX = (dir > 0) ? x + triW : x - triW;
            
            p1 = ImVec2(x, center.y - triH * 0.5f);
            p2 = ImVec2(x, center.y + triH * 0.5f);
            p3 = ImVec2(tipX, center.y);

            drawList->AddTriangleFilled(p1, p2, p3, finalCol);
        }
    }
}

void RenderGhostStatusOverlay(const ImVec2& vPos, const ImVec2& vSize, bool isPaused) {
    static PlayPauseOverlay s;
    float dt = ImGui::GetIO().DeltaTime;

    if (!s.initialized) {
        s.last_paused = isPaused;
        s.initialized = true;
        return;
    }

    if (isPaused != s.last_paused) {
        s.last_paused = isPaused;
        s.alpha = 1.0f;
        s.scale = 0.6f; 
    }

    if (s.alpha > 0.0f) {
        s.alpha -= dt * 1.8f;
        s.scale = ImLerp(s.scale, 1.4f, dt * 5.0f); 
    }

    if (s.alpha > 0.001f) {
        ImDrawList* dl = ImGui::GetWindowDrawList(); 
        
        ImVec2 center = ImVec2(vPos.x + vSize.x * 0.5f, vPos.y + vSize.y * 0.5f);
        float baseSize = (vSize.y * 0.08f) * s.scale;

        ImVec4 iconColVec = ImVec4(1.0f, 1.0f, 1.0f, s.alpha * 0.9f);
        ImU32 iconCol = ImGui::ColorConvertFloat4ToU32(iconColVec);
        
        ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, s.alpha * 0.4f));

        dl->AddCircleFilled(center, baseSize * 1.8f, glowCol, 36);

        if (isPaused) {
            float barWidth = baseSize * 0.45f;
            float barHeight = baseSize * 1.1f;
            float gap = baseSize * 0.25f;

            dl->AddRectFilled(
                ImVec2(center.x - barWidth - gap, center.y - barHeight),
                ImVec2(center.x - gap, center.y + barHeight),
                iconCol, 5.0f);
            
            dl->AddRectFilled(
                ImVec2(center.x + gap, center.y - barHeight),
                ImVec2(center.x + barWidth + gap, center.y + barHeight),
                iconCol, 5.0f);
        } 
        else {
            float pSize = baseSize * 1.2f;
            ImVec2 p1 = center + ImVec2(-pSize * 0.6f, -pSize * 0.9f);
            ImVec2 p2 = center + ImVec2(-pSize * 0.6f,  pSize * 0.9f);
            ImVec2 p3 = center + ImVec2( pSize * 1.0f,  0.0f);
            
            dl->AddTriangleFilled(p1 + ImVec2(2,2), p2 + ImVec2(2,2), p3 + ImVec2(2,2), glowCol);
            dl->AddTriangleFilled(p1, p2, p3, iconCol);
        }
    }
}