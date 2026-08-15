#include "PlayerSession.h"
#include "WindowRuntime.h"
#include <SDL.h>
#include <log.h>
PlayerSession::PlayerSession(std::string id) : m_id(std::move(id)) {}

PlayerSession::~PlayerSession() {
    Shutdown();
}

bool PlayerSession::Init(WindowRuntime* runtime) {
    if (!runtime) return false;

    m_player = std::make_unique<Player>();
    if (!m_player->Init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể khởi tạo Player", nullptr);
        return false;
    }

    m_renderer = std::make_unique<PlayBackRender>();

#ifdef RENDER_MPV_THREAD
    RenderThreadInitParams threadParams{};
    threadParams.mpv = m_player->GetHandle();
    threadParams.ownerWindowId = runtime->info.id;
    threadParams.window = runtime->resource.sdlWindow;
    threadParams.graphicsBackend = runtime->resource.graphicsBackend.get();

    threadParams.initialW = (int)runtime->state.geometry.layout.ClientSize.x;
    threadParams.initialH = (int)runtime->state.geometry.layout.ClientSize.y;

    std::any subContext = runtime->resource.graphicsBackend->CreateSubContext(runtime->resource.sdlWindow);
    if (!subContext.has_value()) {
        SDL_Log("Lỗi: Không thể tạo Shared Context cho luồng phụ MPV!");
        return false;
    }
    threadParams.graphicsContext = subContext;

    if (!m_renderer->Init(*m_player, runtime->resource.graphicsBackend.get(), threadParams)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể tạo PlayBackRender", nullptr);
        return false;
    }
#else
    if (!m_renderer->Init(*m_player, runtime->resource.graphicsBackend.get())) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể tạo PlayBackRender", nullptr);
        return false;
    }
#endif

    m_state = std::make_unique<PlayerStateSystem>();

    // --- KÍCH HOẠT VÀ KHỞI TẠO AUDIO PIPELINE ---
    m_audio = std::make_unique<Audio>();
    if (!m_audio->Init(m_player->GetHandle(), m_state.get())) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, "Cảnh báo: Không thể khởi tạo Audio Pipeline!");
    }

    m_commander = std::make_unique<PlaybackCommand>(*m_player);
    m_property = std::make_unique<PlayBackProperty>(*m_player);

    m_audioFilterManager = std::make_unique<AudioFilterManager>();
    m_audioFilterManager->Init(m_player->GetHandle(), m_state.get());

    m_shaderManager = std::make_unique<ShaderManager>();
    m_shaderManager->Init(m_player->GetHandle());

    m_observer = std::make_unique<PlaybackObserver>(*m_player, *m_state, *m_commander);
    m_observer->Init();

    return true;
}

void PlayerSession::Shutdown() {
    // Shutdown Renderer
    if (m_renderer) m_renderer->Shutdown();
    m_renderer.reset();

    // Shutdown Audio Pipeline sạch sẽ trước khi hủy Player Handle
    if (m_audio) m_audio->Shutdown();
    m_audio.reset();

    m_observer.reset();
    m_commander.reset();
    m_property.reset();

    if (m_player) m_player->Shutdown();
    m_player.reset();
}