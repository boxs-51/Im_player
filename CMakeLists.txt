cmake_minimum_required(VERSION 4.2)
project(Im_player LANGUAGES C CXX)

# BRG-1 canonical build contract:
# - Windows x64 / MSVC ABI
# - Visual Studio 18 2026 + v145 toolset via CMakePresets.json
# - vcpkg owns SDL2 / FreeType / CPR / OpenSSL
# - provisioned libmpv owns mpv headers + runtime/import library
# See docs/BUILDING.md and deps.lock.json.

if(NOT WIN32)
    message(FATAL_ERROR "Im_player BRG-1 baseline supports Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "Im_player BRG-1 baseline requires the MSVC ABI/toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ImPlayerDependencies.cmake)

# Source graph decision (BRG-1):
# native/src/api/api_manager.cpp is intentionally EXCLUDED-PROTOTYPE.
# Activating it requires a dedicated Issue because it changes runtime behavior.
add_executable(Im_player WIN32
    native/src/main1.cpp
    native/src/utils.cpp
    native/src/FontManager.cpp
    native/include/gl3w.c
    native/src/globals.cpp
    native/src/hotkey_handler.cpp
    native/src/notification.cpp
    native/include/wintoastlib.cpp
    native/src/settings_manager.cpp
    native/include/stb_impl.cpp
    native/include/pffft/pffft.c
    native/include/pffft/pffft_common.c

    native/src/windows/main/mpv_ui.cpp
    native/src/windows/main/mpv_ui_settings.cpp

    native/src/player/audio/filter/af_m_core.cpp
    native/src/player/audio/filter/af_m_ai.cpp
    native/src/player/audio/filter/af_m_io.cpp
    native/src/player/audio/filter/af_m_safety.cpp
    native/src/player/audio/filter/af_m_sync.cpp
    native/src/player/audio/filter/af_m_analyzer.cpp
    native/src/player/audio/filter/af_m_context.cpp
    native/src/player/audio/filter/af_m_dispatcher.cpp
    native/src/player/audio/filter/af_m_engine.cpp
    native/src/player/audio/AudioCaptureManager.cpp

    native/src/player/video/filter/video_filter_manager.cpp
    native/src/player/shaders/shader_manager.cpp
    native/src/player/scripts/script_manager.cpp
    native/src/player/command/PlaybackCommand.cpp
    native/src/player/event/PlaybackObserver.cpp
    native/src/player/event/BuildFormatYTDLP.cpp
    native/src/player/player/Player.cpp
    native/src/player/property/PlayBackProperty.cpp
    native/src/player/render/PlayBackRender.cpp
    native/src/player/render/OpenGLFrameBufferPool.cpp
    native/src/player/render/PlayBackRenderThread.cpp
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

    native/include/imgui/imgui.cpp
    native/include/imgui/imgui_draw.cpp
    native/include/imgui/imgui_tables.cpp
    native/include/imgui/imgui_widgets.cpp
    native/include/imgui/imgui_demo.cpp
    native/include/imgui/misc/freetype/imgui_freetype.cpp
    native/include/imgui/backends/imgui_impl_sdl2.cpp
    native/include/imgui/backends/imgui_impl_opengl3.cpp
    native/include/imgui/backends/imgui_impl_dx11.cpp

    native/src/popup/popup_about_video.cpp
    native/src/popup/popup_setting.cpp
    native/src/popup/popup_url.cpp
    native/src/popup/popup.cpp
    native/src/popup/popup_test.cpp
    native/src/popup/reusable_popup.cpp
    native/src/popup/sidebar_popup.cpp

    native/src/backends/client_backend.cpp
    native/src/backends/backend.cpp
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
    native/src/gui/widgets/gui_table.cpp
    native/src/gui/widgets/gui_status.cpp

    native/src/windows/Evevnt.cpp
    native/src/windows/RuntimeFontController.cpp

    native/src/player/audio/AudioOutputWorker.cpp
    native/src/player/audio/Audio.cpp
    native/src/player/audio/AudioProcessor.cpp
    native/src/player/audio/SdlAudioDevice.cpp
)

target_compile_definitions(Im_player PRIVATE
    SDL_MAIN_HANDLED
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    CUSTOM_TITLEBAR
    RENDER_MPV_THREAD
    PFFFT_EXPORTS
    ENABLE_LOG
)

# Package-owned include directories are inserted BEFORE native/include so
# vendored legacy copies cannot shadow the canonical dependency headers.
implayer_apply_dependency_includes(Im_player)

target_include_directories(Im_player PRIVATE
    native/include
    native/src

    native/src/player
    native/src/player/shaders
    native/src/player/scripts
    native/src/player/audio
    native/src/player/audio/filter
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
    native/include/compat
    native/include/curl
    native/include/pffft
    native/include/pffft/simd

    native/src/windows
    native/src/windows/main
    native/src/windows/main/ui
    native/src/windows/sub
    native/src/popup
    native/src/threads
    native/src/backends
    native/src/common

    native/include/imgui/misc/freetype
    native/include/imgui/backends
    native/include/imgui
)

target_link_libraries(Im_player PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    Freetype::Freetype
    cpr::cpr
    OpenSSL::Crypto
    OpenSSL::SSL
    mpv::mpv
    opengl32
    winhttp
    dwmapi
    d3d11
    dxgi
)

set_target_properties(Im_player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
)

# libmpv is not vcpkg-managed, so make its runtime closure explicit.
add_custom_command(TARGET Im_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mpv::mpv>"
        "$<TARGET_FILE_DIR:Im_player>"
    COMMENT "Copying pinned libmpv runtime"
)

# Keep the current Win32/main entry contract unchanged.
target_link_options(Im_player PRIVATE
    /SUBSYSTEM:WINDOWS
    /ENTRY:mainCRTStartup
)


# BRG-3 focused lifetime regression target.
include(CTest)
if(BUILD_TESTING)
    add_executable(render_callback_lifetime_test
        tests/render_callback_lifetime_test.cpp
    )
    target_include_directories(render_callback_lifetime_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(render_callback_lifetime_test PRIVATE cxx_std_17)

    add_test(
        NAME brg3_render_callback_lifetime_gate
        COMMAND render_callback_lifetime_test
    )
    set_tests_properties(
        brg3_render_callback_lifetime_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(audio_visualizer_snapshot_test
        tests/audio_visualizer_snapshot_test.cpp
    )
    target_include_directories(audio_visualizer_snapshot_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(audio_visualizer_snapshot_test PRIVATE cxx_std_17)

    add_test(
        NAME issue20_audio_visualizer_snapshot_gate
        COMMAND audio_visualizer_snapshot_test
    )
    set_tests_properties(
        issue20_audio_visualizer_snapshot_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(pcm_frame_accumulator_test
        tests/pcm_frame_accumulator_test.cpp
    )
    target_include_directories(pcm_frame_accumulator_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(pcm_frame_accumulator_test PRIVATE cxx_std_17)

    add_test(
        NAME issue26_pcm_frame_accumulator_gate
        COMMAND pcm_frame_accumulator_test
    )
    set_tests_properties(
        issue26_pcm_frame_accumulator_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(audio_telemetry_math_test
        tests/audio_telemetry_math_test.cpp
    )
    target_include_directories(audio_telemetry_math_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(audio_telemetry_math_test PRIVATE cxx_std_17)

    add_test(
        NAME issue26_audio_telemetry_math_gate
        COMMAND audio_telemetry_math_test
    )
    set_tests_properties(
        issue26_audio_telemetry_math_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(pcm_realtime_pacer_test
        tests/pcm_realtime_pacer_test.cpp
    )
    target_include_directories(pcm_realtime_pacer_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(pcm_realtime_pacer_test PRIVATE cxx_std_17)

    add_test(
        NAME issue26_pcm_realtime_pacer_gate
        COMMAND pcm_realtime_pacer_test
    )
    set_tests_properties(
        issue26_pcm_realtime_pacer_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(audio_media_clock_test
        tests/audio_media_clock_test.cpp
    )
    target_include_directories(audio_media_clock_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(audio_media_clock_test PRIVATE cxx_std_17)

    add_test(
        NAME issue26_audio_media_clock_gate
        COMMAND audio_media_clock_test
    )
    set_tests_properties(
        issue26_audio_media_clock_gate
        PROPERTIES TIMEOUT 30
    )

    add_executable(audio_queue_underflow_detector_test
        tests/audio_queue_underflow_detector_test.cpp
    )
    target_include_directories(audio_queue_underflow_detector_test PRIVATE
        "${CMAKE_SOURCE_DIR}/native/src"
    )
    target_compile_features(audio_queue_underflow_detector_test PRIVATE cxx_std_17)

    add_test(
        NAME issue26_audio_queue_underflow_gate
        COMMAND audio_queue_underflow_detector_test
    )
    set_tests_properties(
        issue26_audio_queue_underflow_gate
        PROPERTIES TIMEOUT 30
    )
endif()
