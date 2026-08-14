#pragma once

#include "player/player/Player.h"
#include "player/render/PlayBackRender.h"
#include "player/render/PlayBackRenderThread.h"
#include "player/event/PlaybackObserver.h"
#include "player/command/PlaybackCommand.h"
#include "player/property/PlayBackProperty.h"
#include "player/PlayerStateSystem.h"
#include "player/audio/filter/af_m.h"
#include "player/audio/AudioCaptureManager.h"
#include "player/video/filter/video_filter_manager.h"
#include "player/shaders/shaders_manager.h"


#include <memory>
#include <string>



class ShaderManager;
class VideoFilterManager;

class PlayerSession {
public:
    PlayerSession(std::string id);
    ~PlayerSession();

    bool Init(WindowRuntime* runtime);
    void Shutdown();

    const std::string& GetId() const { return m_id; }

    Player* GetPlayer() const { return m_player.get(); }
    PlayBackRender* GetRenderer() const { return m_renderer.get(); }
    PlaybackObserver* GetObserver() const { return m_observer.get(); }
    PlaybackCommand* GetCommander() const { return m_commander.get(); }
    PlayBackProperty* GetProperty() const { return m_property.get(); }
    PlayerStateSystem* GetState() const { return m_state.get(); }

    PlayBackRenderThread* GetRenderThread() const { 
        return m_renderer ? m_renderer->GetRenderThread() : nullptr; 
    }

    AudioFilterManager* GetAudioFilterManager() const { return m_audioFilterManager.get(); }
    AudioCaptureManager* GetAudioCaptureManager() const { return m_audioCaptureManager.get(); }
    VideoFilterManager* GetVideoFilterManager() const { return m_videoFilterManager.get(); }

    ShaderManager* GetShaderManager() const { return m_shaderManager.get(); }

    /**
     * @brief Thực thi callback an toàn nếu `this` (Session) hợp lệ.
     * Tránh crash khi gọi chuỗi từ WindowResource hoặc PlayerManager.
     */
    template <typename Func>
    void SafeExecute(Func&& func) {
        if (this) {
            func(*this);
        }
    }


private:
    std::string m_id;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<PlayBackRender> m_renderer;
    std::unique_ptr<PlaybackObserver> m_observer;
    std::unique_ptr<PlaybackCommand> m_commander;
    std::unique_ptr<PlayBackProperty> m_property;
    std::unique_ptr<PlayerStateSystem> m_state;

    std::unique_ptr<AudioFilterManager> m_audioFilterManager;
    std::unique_ptr<AudioCaptureManager> m_audioCaptureManager;
    std::unique_ptr<VideoFilterManager> m_videoFilterManager;

    std::unique_ptr<ShaderManager> m_shaderManager;
};