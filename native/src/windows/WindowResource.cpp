#include "WindowResource.h"

#include "player/session/PlayerManager.h"
PlayerSession* WindowResource::GetPlayerSession() const
{
    if (playersessionid.empty()) {
        return nullptr;
    }
    // Tra cứu trực tiếp từ PlayerManager. Nếu Session không còn tồn tại, tự động trả về nullptr an toàn
    return PlayerManager::GetInstance().GetSession(playersessionid);
}