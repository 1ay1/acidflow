cmake_minimum_required(VERSION 3.28)
project(acidflow VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    set(CMAKE_CXX_STANDARD 23)
else()
    set(CMAKE_CXX_STANDARD 26)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── maya framework ──────────────────────────────────────────────────────────
# Expect a sibling checkout at ../maya. If that ever becomes inconvenient,
# swap this for FetchContent_Declare(maya ...).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../maya ${CMAKE_CURRENT_BINARY_DIR}/maya)

# ── Platform-specific audio backend ─────────────────────────────────────────
# Every backend implements the C interface in src/audio.hpp (acid_start /
# acid_stop). The shared DSP lives in engine.cpp and is platform-agnostic.
set(ACIDFLOW_SOURCES
    src/main.cpp
    src/engine.cpp
)

if(APPLE)
    list(APPEND ACIDFLOW_SOURCES src/audio_coreaudio.cpp src/midi_stub.cpp)
    find_library(AUDIO_TOOLBOX   AudioToolbox   REQUIRED)
    find_library(CORE_AUDIO      CoreAudio      REQUIRED)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    set(ACIDFLOW_AUDIO_LIBS
        ${AUDIO_TOOLBOX}
        ${CORE_AUDIO}
        ${CORE_FOUNDATION}
    )
elseif(WIN32)
    list(APPEND ACIDFLOW_SOURCES src/audio_wasapi.cpp src/midi_stub.cpp)
    # WASAPI lives in ole32 (CoCreateInstance/MMDeviceEnumerator) and the
    # audio client headers ship with the Windows SDK. No explicit audio lib
    # needed — link ole32 for COM and winmm for timing helpers.
    set(ACIDFLOW_AUDIO_LIBS ole32 winmm)
elseif(UNIX)
    list(APPEND ACIDFLOW_SOURCES src/audio_alsa.cpp src/midi_alsa.cpp)
    # Prefer pkg-config (handles multiarch include paths) but fall back to a
    # plain library lookup for minimal distros.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
        set(ACIDFLOW_AUDIO_LIBS PkgConfig::ALSA)
    else()
        find_library(ALSA_LIB asound REQUIRED)
        set(ACIDFLOW_AUDIO_LIBS ${ALSA_LIB})
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: no audio backend available")
endif()

add_executable(acidflow ${ACIDFLOW_SOURCES})
target_link_libraries(acidflow PRIVATE maya ${ACIDFLOW_AUDIO_LIBS})
target_include_directories(acidflow PRIVATE src)
target_compile_options(acidflow PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native -funroll-loops>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /utf-8>
)
target_compile_definitions(acidflow PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
)

# Worker thread in audio_alsa.cpp / audio_wasapi.cpp needs pthreads on
# *nix; Windows uses the Win32 thread API inside std::thread and needs no
# extra lib. Harmless on Apple since pthreads are always linked.
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(acidflow PRIVATE Threads::Threads)
endif()
