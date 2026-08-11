#pragma once

#include <memory>
#include <string>

#include "player/player/Player.h"
#include "player/render/PlayBackRender.h"
#include "player/event/PlaybackObserver.h"
#include "player/command/PlaybackCommand.h"
#include "player/property/PlayBackProperty.h"
#include "player/render/PlayBackRenderThread.h"
#include "player/PlayerStateSystem.h"
#include "player/audio/filter/af_m.h"
#include "player/video/filter/video_filter_manager.h"

#include "windows/WindowTemplate.h"

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
    PlaybackCommandDispatcher* GetCommander() const { return m_commander.get(); }
    PlayBackProperty* GetProperty() const { return m_property.get(); }
    PlayerStateSystem* GetState() const { return m_state.get(); }
    std::shared_ptr<PlayBackRenderThread> GetRenderThread() const { return m_renderThread; }

    AudioFilterManager* GetAudioFilterManager() const { return m_audioFilterManager.get(); }
    VideoFilterManager* GetVideoFilterManager() const { return m_videoFilterManager.get(); }

private:
    std::string m_id;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<PlayBackRender> m_renderer;
    std::unique_ptr<PlaybackObserver> m_observer;
    std::unique_ptr<PlaybackCommandDispatcher> m_commander;
    std::unique_ptr<PlayBackProperty> m_property;
    std::unique_ptr<PlayerStateSystem> m_state;
    std::shared_ptr<PlayBackRenderThread> m_renderThread;

    std::unique_ptr<AudioFilterManager> m_audioFilterManager;
    std::unique_ptr<VideoFilterManager> m_videoFilterManager;

};