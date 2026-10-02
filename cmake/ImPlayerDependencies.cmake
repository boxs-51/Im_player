# Canonical dependency boundary for BRG-1.
# This file intentionally contains no network/download logic.
# scripts/bootstrap-deps.ps1 provisions the pinned inputs before configure.

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    message(FATAL_ERROR "VCPKG_TARGET_TRIPLET is not set. Configure with the canonical CMake preset.")
endif()

if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows")
    message(FATAL_ERROR "BRG-1 supports only VCPKG_TARGET_TRIPLET=x64-windows.")
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

set(IMPLAYER_MPV_ROOT
    "${CMAKE_SOURCE_DIR}/deps/mpv"
    CACHE PATH
    "Root of the pinned libmpv developer package provisioned by bootstrap-deps.ps1"
)

set(IMPLAYER_MPV_INCLUDE_DIR "${IMPLAYER_MPV_ROOT}/include")
set(IMPLAYER_MPV_DLL "${IMPLAYER_MPV_ROOT}/libmpv-2.dll")
set(IMPLAYER_MPV_IMPLIB "${IMPLAYER_MPV_ROOT}/libmpv-2.dll.lib")

foreach(_required
    "${IMPLAYER_MPV_INCLUDE_DIR}/mpv/client.h"
    "${IMPLAYER_MPV_INCLUDE_DIR}/mpv/render.h"
    "${IMPLAYER_MPV_DLL}"
    "${IMPLAYER_MPV_IMPLIB}"
)
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "Missing pinned libmpv dependency: ${_required}\n"
            "Run: powershell -ExecutionPolicy Bypass -File scripts/bootstrap-deps.ps1"
        )
    endif()
endforeach()

add_library(mpv::mpv SHARED IMPORTED GLOBAL)
set_target_properties(mpv::mpv PROPERTIES
    IMPORTED_LOCATION "${IMPLAYER_MPV_DLL}"
    IMPORTED_IMPLIB "${IMPLAYER_MPV_IMPLIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${IMPLAYER_MPV_INCLUDE_DIR}"
)

# Make canonical package headers precede native/include, which still contains
# legacy vendored copies retained for historical compatibility/audit only.
function(implayer_apply_dependency_includes target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Unknown target passed to implayer_apply_dependency_includes: ${target_name}")
    endif()

    foreach(dep_target
        mpv::mpv
        SDL2::SDL2
        Freetype::Freetype
        cpr::cpr
        OpenSSL::SSL
    )
        if(NOT TARGET "${dep_target}")
            message(FATAL_ERROR "Required dependency target is missing: ${dep_target}")
        endif()

        get_target_property(_dep_includes "${dep_target}" INTERFACE_INCLUDE_DIRECTORIES)
        if(_dep_includes)
            target_include_directories("${target_name}" BEFORE PRIVATE ${_dep_includes})
        endif()
    endforeach()
endfunction()
