cmake_minimum_required(VERSION 3.20)
project(imgui_player)

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
add_executable(imgui_player 

    native/src/main.cpp
    native/src/utils.cpp
    native/src/FontManager.cpp
    native/src/gl3w.c
    native/src/globals.cpp
    native/src/hotkey_handler.cpp

    native/src/mpv/mpv_basic_formats.cpp
    native/src/mpv/mpv_controller.cpp
    native/src/mpv/mpv_ui.cpp
    native/src/mpv/mpv_render_video.cpp
    native/src/mpv/mpv_settings.cpp
    native/src/mpv/mpv_ui_settings.cpp

    native/src/UIStyleManager.cpp

    native/src/windows/windows_borderless.cpp
    native/src/windows/windows_custom_titlebar.cpp

    native/imgui/imgui.cpp
    native/imgui/imgui_draw.cpp
    native/imgui/imgui_tables.cpp
    native/imgui/imgui_widgets.cpp
    native/imgui/imgui_demo.cpp

    native/imgui/misc/freetype/imgui_freetype.cpp

    native/imgui/backends/imgui_impl_sdl2.cpp
    native/imgui/backends/imgui_impl_opengl3.cpp

    native/src/popup/popup_about_video.cpp
    native/src/popup/popup_setting.cpp
    native/src/popup/popup_url.cpp
    native/src/popup/reusable_popup.cpp
    native/src/popup/sidebar_popup.cpp

    native/src/service/key_manager_server.cpp
    native/src/service/resolution_service.cpp
    native/src/service/youtube_sevice.cpp
    native/src/service/services.cpp
    native/src/service/client_backend.cpp

    native/src/threads/thread_manager.cpp
    native/src/threads/thread.cpp

)

# ================== WINDOWS DEFINES ==================
target_compile_definitions(imgui_player PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)
# Include path
target_include_directories(imgui_player PRIVATE
    native/include        
    native/include/mpv      
    native/include/nlohmann  
    native/include/KHR
    native/include/GL
    native/include/freetype2
    native/include/SDL2
    native/include/compat
    native/include/curl
    native/include/cpr
    native/include/openssl
    native/include/windows
    native/include/popup
    native/include/threads

    native/imgui/misc/freetype
    native/imgui/backends
    native/imgui

)

target_link_libraries(imgui_player PRIVATE
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
)

set_target_properties(imgui_player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/Release
)

if(WIN32)
    add_custom_command(TARGET imgui_player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/libs/libmpv-2.dll" # Copy thủ công mpv
        "$<TARGET_FILE_DIR:imgui_player>"
    )
    
    # Lệnh này yêu cầu vcpkg tự copy các file phụ thuộc khác
    set_target_properties(imgui_player PROPERTIES VS_USER_PROPS_CXX_DEFAULT "true")
endif()
