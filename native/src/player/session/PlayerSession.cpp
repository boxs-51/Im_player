#include "PlayerSession.h"
#include "MainWindowState.h"
#include <SDL.h>

PlayerSession::PlayerSession(std::string id) : m_id(std::move(id)) {}

PlayerSession::~PlayerSession() {
    Shutdown();
}

bool PlayerSession::Init(WindowRuntime* runtime) {
    m_player = std::make_unique<Player>();
    if (!m_player->Init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể khởi tạo Player", nullptr);
        return false;
    }

#ifdef RENDER_MPV_THREAD
    m_renderThread = std::make_shared<PlayBackRenderThread>();
#endif

    m_renderer = std::make_unique<PlayBackRender>();
    if (!m_renderer->Init(*m_player, runtime->resource.graphicsBackend.get(), m_renderThread)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể tạo PlayBackRender", nullptr);
        return false;
    }
    m_state = std::make_unique<PlayerStateSystem>();
    m_commander = std::make_unique<PlaybackCommandDispatcher>(*m_player);
    m_property = std::make_unique<PlayBackProperty>(*m_player);

    m_audioFilterManager = std::make_unique<AudioFilterManager>();
    m_audioFilterManager->Init(m_player->GetHandle(), m_state.get());

    m_observer = std::make_unique<PlaybackObserver>(*m_player, *m_state, *m_commander);
    m_observer->Init();

#ifdef RENDER_MPV_THREAD
    // Cấu hình trạng thái ban đầu cho luồng render
    m_renderThread->state.mpv = m_player->GetHandle(); // <-- THÊM DÒNG NÀY
    m_renderThread->state.ownerWindowId = runtime->info.id;
    m_renderThread->state.window = runtime->resource.sdlWindow;
    m_renderThread->state.render_ctx = m_renderer->GetContext();
    m_renderThread->state.graphicsBackend = runtime->resource.graphicsBackend.get(); // Lưu con trỏ backend
    m_renderThread->state.Audio_visualizers = m_renderer->IsAudioVisualizerEnabled();

    if (runtime->properties.Contains("Layout")) {
        auto layout = runtime->properties.GetValue<WindowLayout>("Layout");
        m_renderThread->state.surface.drawW = (int)layout.ClientSize.x;
        m_renderThread->state.surface.drawH = (int)layout.ClientSize.y;
    }
    std::any subContext = runtime->resource.graphicsBackend->CreateSubContext(runtime->resource.sdlWindow);
    if (!subContext.has_value()) {
        SDL_Log("Lỗi: Không thể tạo Shared Context cho luồng phụ MPV!");
        m_renderThread.reset(); // Hủy luồng nếu không tạo được context
    } else {
        m_renderThread->state.graphicsContext = subContext;
        m_renderThread->Start(); // Không truyền runtime vào nữa
    }
#endif

    return true;
}

void PlayerSession::Shutdown() {
#ifdef RENDER_MPV_THREAD
    if (m_renderThread) {
        m_renderThread->Stop();
        m_renderThread.reset();
    }
#endif

    // Các thành phần khác phải được shutdown trước player
    if (m_renderer) m_renderer->Shutdown();
    m_renderer.reset();
    m_observer.reset();
    m_commander.reset();
    m_property.reset();

    if (m_player) m_player->Shutdown();
    m_player.reset();
}