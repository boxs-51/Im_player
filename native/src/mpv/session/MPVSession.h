#pragma once

#include <memory>
#include <string>

#include "mpv/player/MPVPlayer.h"
#include "mpv/render/MPVRender.h"
#include "mpv/event/MPVObserver.h"
#include "mpv/command/MPVCommand.h"
#include "mpv/property/MPVProperty.h"
#include "mpv/render/MPVRenderThread.h"
#include "MPVStateSystem.h"
#include "WindowTemplate.h"

class MPVSession {
public:
    MPVSession(std::string id);
    ~MPVSession();

    bool Init(WindowRuntime* runtime);
    void Shutdown();

    const std::string& GetId() const { return m_id; }

    MPVPlayer* GetPlayer() const { return m_player.get(); }
    MPVRender* GetRenderer() const { return m_renderer.get(); }
    MPVObserver* GetObserver() const { return m_observer.get(); }
    MPVCommandDispatcher* GetCommander() const { return m_commander.get(); }
    MPVProperty* GetProperty() const { return m_property.get(); }
    MPVStateSystem* GetState() const { return m_state.get(); }
    std::shared_ptr<MPVRenderThread> GetRenderThread() const { return m_renderThread; }

private:
    std::string m_id;
    std::unique_ptr<MPVPlayer> m_player;
    std::unique_ptr<MPVRender> m_renderer;
    std::unique_ptr<MPVObserver> m_observer;
    std::unique_ptr<MPVCommandDispatcher> m_commander;
    std::unique_ptr<MPVProperty> m_property;
    std::unique_ptr<MPVStateSystem> m_state;
    std::shared_ptr<MPVRenderThread> m_renderThread;
};