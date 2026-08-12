#pragma once

#include <memory>
#include <string>

class Player;
class PlayBackRender;
class PlaybackObserver;
class PlaybackCommandDispatcher;
class PlayBackProperty;
class PlayBackRenderThread;
class AudioFilterManager;
class ideoFilterManager;
class ShaderManager;

class class PlayerSession {
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

    ShaderManager* GetShaderManager() const { return m_shaderManager.get(); }


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

    std::unique_ptr<ShaderManager> m_shaderManager;
};