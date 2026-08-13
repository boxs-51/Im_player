cmake_minimum_required(VERSION 3.20)
project(Im_player)

# ================== C++ CONFIG ==================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(VCPKG_TARGET_TRIPLET x64-windows)

find_package(SDL2 CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# ================== EXECUTABLE ==================
add_executable(Im_player WIN32
    #
    native/src/main1.cpp
    native/src/utils.cpp
    native/src/FontManager.cpp
    native/src/gl3w.c
    native/src/globals.cpp
    native/src/hotkey_handler.cpp
    native/src/notification.cpp
    native/src/wintoastlib.cpp
    native/src/settings_manager.cpp
    native/src/stb_impl.cpp
    # 
    native/src/windows/main/mpv_ui.cpp
    native/src/windows/main/mpv_ui_settings.cpp
    #
    native/src/player/audio/filter/af_m_core.cpp
    native/src/player/audio/filter/af_m_ai.cpp
    native/src/player/audio/filter/af_m_io.cpp
    native/src/player/audio/filter/af_m_safety.cpp
    native/src/player/audio/filter/af_m_sync.cpp
    native/src/player/audio/filter/af_m_analyzer.cpp
    native/src/player/audio/filter/af_m_context.cpp
    native/src/player/audio/filter/af_m_dispatcher.cpp
    native/src/player/audio/filter/af_m_engine.cpp
    #
    native/src/player/video/filter/video_filter_manager.cpp
    #
    native/src/player/shaders/shader_manager.cpp
    #
    native/src/player/scripts/script_manager.cpp
    #
    native/src/player/command/PlaybackCommand.cpp
    #
    native/src/player/event/PlaybackObserver.cpp
    native/src/player/event/BuildFormatYTDLP.cpp
    #
    native/src/player/player/Player.cpp
    #
    native/src/player/property/PlayBackProperty.cpp
    #
    native/src/player/render/PlayBackRender.cpp
    native/src/player/render/OpenGLFrameBufferPool.cpp
    native/src/player/render/PlayBackRenderThread.cpp
    #
    native/src/player/session/PlayerSession.cpp
    native/src/player/session/PlayerManager.cpp

    native/src/windows/main/MainWindowRenderer.cpp
    native/src/windows/sub/MockSubWindowRenderer.cpp

    native/src/windows/WindowController.cpp
    native/src/windows/WindowProcHook.cpp
    native/src/windows/UIRenderThread.cpp
    native/src/windows/WindowSharedGroup.cpp
    native/src/windows/WindowInitializer.cpp
    native/src/windows/WindowRelation.cpp
    native/src/windows/NativeWindowRegistrar.cpp
    #
    native/src/gui/gui.cpp
    native/src/gui/gui_widgets.cpp
    #
    native/imgui/imgui.cpp
    native/imgui/imgui_draw.cpp
    native/imgui/imgui_tables.cpp
    native/imgui/imgui_widgets.cpp
    native/imgui/imgui_demo.cpp
    native/imgui/misc/freetype/imgui_freetype.cpp
    native/imgui/backends/imgui_impl_sdl2.cpp
    native/imgui/backends/imgui_impl_opengl3.cpp
    native/imgui/backends/imgui_impl_dx11.cpp
    #
    native/src/popup/popup_about_video.cpp
    native/src/popup/popup_setting.cpp
    native/src/popup/popup_url.cpp
    native/src/popup/popup.cpp
    native/src/popup/popup_test.cpp
    native/src/popup/reusable_popup.cpp
    native/src/popup/sidebar_popup.cpp
    #
    native/src/backends/client_backend.cpp
    native/src/backends/backend.cpp
    #
    native/src/threads/thread_manager.cpp

    native/src/windows/main/ui/ui_controls.cpp
    native/src/windows/main/ui/ui_overlays.cpp
    native/src/windows/main/ui/ui_settings.cpp
    native/src/windows/main/ui/ui_widgets.cpp
    native/src/windows/WindowResource.cpp
    native/src/windows/main/ui/ui_title_bar.cpp

    native/src/gui/core/gui_theme.cpp
    native/src/gui/draw/gui_draw_helpers.cpp
    native/src/gui/draw/icons/icon_media.cpp
    native/src/gui/draw/icons/icon_system.cpp
    native/src/gui/draw/icons/icon_volume.cpp
    native/src/gui/widgets/gui_buttons.cpp
    native/src/gui/widgets/gui_containers.cpp
    native/src/gui/widgets/gui_inputs.cpp
    native/src/gui/widgets/gui_slider.cpp
    native/src/gui/widgets/gui_text.cpp
    native/src/gui/widgets/gui_tooltip.cpp
    native/src/windows/Evevnt.cpp
    native/src/windows/RuntimeFontController.cpp
    
    
)

# ================== WINDOWS DEFINES ==================
target_compile_definitions(Im_player PRIVATE

    SDL_MAIN_HANDLED
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    CUSTOM_TITLEBAR
    RENDER_MPV_THREAD
)
# Include path
target_include_directories(Im_player PRIVATE
    native/include   
    native/src 

    native/include/mpv      
    native/src/player  
    native/src/player/shaders
    native/src/player/scripts
    native/src/player/audio
    native/src/player/video
    native/src/player/command
    native/src/player/event
    native/src/player/player
    native/src/player/property
    native/src/player/render
    native/src/player/session

    native/src/gui
    native/src/gui/core
    native/src/gui/widgets
    native/src/gui/draw

    native/include/nlohmann  
    native/include/KHR
    native/include/GL
    native/include/freetype2
    native/include/SDL2
    native/include/compat
    native/include/curl
    native/include/cpr
    native/include/openssl

    native/src/windows
    native/src/windows/main
    native/src/windows/main/ui
    native/src/windows/sub
    native/src/popup
    native/src/threads
    native/src/backends
    native/src/common

    native/imgui/misc/freetype
    native/imgui/backends
    native/imgui
)

target_link_libraries(Im_player PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    ${CMAKE_SOURCE_DIR}/libs/libmpv.dll.a
    opengl32
    winhttp
    dwmapi
    freetype
    cpr::cpr
    OpenSSL::Crypto
    OpenSSL::SSL
    d3d11
    dxgi 
)

set_target_properties(Im_player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/Release
)

if(WIN32)
    add_custom_command(TARGET Im_player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/libs/libmpv-2.dll" # Copy thủ công mpv
        "$<TARGET_FILE_DIR:Im_player>"
    )
    
    # Lệnh này yêu cầu vcpkg tự copy các file phụ thuộc khác
    set_target_properties(Im_player PROPERTIES VS_USER_PROPS_CXX_DEFAULT "true")

    set_target_properties(Im_player PROPERTIES 
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()
