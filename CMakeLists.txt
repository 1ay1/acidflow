cmake_minimum_required(VERSION 3.28)
project(acidflow VERSION 0.2.0 LANGUAGES CXX)

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

option(ACIDFLOW_BUILD_APP    "Build the terminal app (needs ../maya)"       ON)
option(ACIDFLOW_BUILD_TESTS  "Build the test suite"                         ON)
option(ACIDFLOW_BUILD_CLAP   "Build the CLAP plugin (fetches the CLAP SDK)" OFF)
option(ACIDFLOW_NATIVE       "Tune for this CPU (-march=native)"            ON)
option(ACIDFLOW_STATIC_RUNTIME "Link libstdc++/libgcc statically (gcc, for release builds)" OFF)

# Only on gcc links, so Apple clang's C compiler checks don't see the flags.
if(ACIDFLOW_STATIC_RUNTIME)
    add_link_options($<$<CXX_COMPILER_ID:GNU>:-static-libstdc++$<SEMICOLON>-static-libgcc>)
endif()

# Shared compile options for everything that runs DSP.
add_library(acid_flags INTERFACE)
target_compile_options(acid_flags INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -funroll-loops>
    $<$<AND:$<BOOL:${ACIDFLOW_NATIVE}>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /utf-8>)
target_compile_definitions(acid_flags INTERFACE $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>)

# Strict warnings for our own DSP code (maya's headers trip -Wextra's
# missing-field-initializer check, so the app doesn't get these).
add_library(acid_warnings INTERFACE)
target_compile_options(acid_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>)

# ── Engine ──────────────────────────────────────────────────────────────────
# The DSP and sequencer, with no terminal or device dependency. Linked by the
# app, the offline renderer, the tests and the plugin.
add_library(acid_engine STATIC src/engine.cpp)
target_include_directories(acid_engine PUBLIC src)
target_link_libraries(acid_engine PUBLIC acid_flags PRIVATE acid_warnings)
set_target_properties(acid_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)


add_executable(acid_render tools/acid_render.cpp)
target_link_libraries(acid_render PRIVATE acid_engine acid_warnings)

# ── App ─────────────────────────────────────────────────────────────────────
if(ACIDFLOW_BUILD_APP)
    # Expect a sibling checkout at ../maya. We only need the library and
    # maya::app (the jaal runtime), not its demos or tests.
    set(MAYA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MAYA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    # Portable builds must not let maya bake in the host CPU either.
    if(NOT ACIDFLOW_NATIVE OR CMAKE_CROSSCOMPILING)
        set(MAYA_NATIVE_TUNING OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../maya ${CMAKE_CURRENT_BINARY_DIR}/maya)

    # Every backend implements acid_start/acid_stop/acid_audio_driver and
    # acid_midi_start/acid_midi_stop from src/audio.hpp.
    set(ACIDFLOW_PLATFORM_SOURCES)
    set(ACIDFLOW_PLATFORM_LIBS)
    if(APPLE)
        # CoreMIDI's headers use blocks, which gcc can't parse, so those calls
        # sit in a C file. Build it with Apple's clang (CC=clang).
        enable_language(C)
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR "Set CC=clang on macOS: gcc can't compile CoreMIDI's headers.")
        endif()
        list(APPEND ACIDFLOW_PLATFORM_SOURCES
            src/audio_coreaudio.cpp src/midi_coremidi.cpp src/coremidi_shim.c)
        find_library(AUDIO_TOOLBOX   AudioToolbox   REQUIRED)
        find_library(CORE_AUDIO      CoreAudio      REQUIRED)
        find_library(CORE_MIDI       CoreMIDI       REQUIRED)
        find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
        list(APPEND ACIDFLOW_PLATFORM_LIBS ${AUDIO_TOOLBOX} ${CORE_AUDIO} ${CORE_MIDI} ${CORE_FOUNDATION})
    elseif(WIN32)
        list(APPEND ACIDFLOW_PLATFORM_SOURCES src/audio_wasapi.cpp src/midi_winmm.cpp)
        list(APPEND ACIDFLOW_PLATFORM_LIBS ole32 winmm)
    elseif(UNIX)
        # JACK is loaded at runtime (dlopen), so only ALSA is a build dep.
        list(APPEND ACIDFLOW_PLATFORM_SOURCES
            src/audio_linux.cpp src/audio_jack.cpp src/audio_alsa.cpp src/midi_alsa.cpp)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
            list(APPEND ACIDFLOW_PLATFORM_LIBS PkgConfig::ALSA)
        else()
            find_library(ALSA_LIB asound REQUIRED)
            list(APPEND ACIDFLOW_PLATFORM_LIBS ${ALSA_LIB})
        endif()
        list(APPEND ACIDFLOW_PLATFORM_LIBS ${CMAKE_DL_LIBS})
    else()
        message(FATAL_ERROR "Unsupported platform: no audio backend available")
    endif()

    # The UI (everything but main and the device backends), shared by the
    # app and the headless UI tests.
    add_library(acid_app STATIC
        src/app/state.cpp src/app/presets.cpp src/app/randomize.cpp src/app/persist.cpp
        src/app/song.cpp src/app/export.cpp src/app/audio.cpp src/app/input.cpp
        src/app/view.cpp src/app/actions.cpp src/app/ui.cpp src/app/menu.cpp src/app/theme_picker.cpp)
    target_include_directories(acid_app PUBLIC src)
    target_link_libraries(acid_app PUBLIC acid_engine maya::app)

    add_executable(acidflow src/main.cpp ${ACIDFLOW_PLATFORM_SOURCES})
    target_link_libraries(acidflow PRIVATE acid_app ${ACIDFLOW_PLATFORM_LIBS})
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(acidflow PRIVATE Threads::Threads)
    endif()
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
if(ACIDFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── CLAP plugin ─────────────────────────────────────────────────────────────
if(ACIDFLOW_BUILD_CLAP)
    add_subdirectory(plugin)
endif()
