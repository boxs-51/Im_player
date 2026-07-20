#pragma once

#include <SDL.h>

#include <windows.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <wrl.h>

namespace SDLUtils{
    inline void SDLX_PushUniqueEvent(const SDL_Event& eventData) {
        SDL_Event existingEvent;
            
        // Kiểm tra xem trong hàng đợi đã có event cùng loại (type) chưa
        // Lưu ý: Nếu là SDL_USEREVENT, bạn có thể kiểm tra thêm cả trường 'code'
        if (SDL_PeepEvents(&existingEvent, 1, SDL_PEEKEVENT, eventData.type, eventData.type) == 0) {
            // Nếu chưa có, copy dữ liệu và đẩy vào
            SDL_Event eventToPush = eventData;
            SDL_PushEvent(&eventToPush);
        }
    }
};