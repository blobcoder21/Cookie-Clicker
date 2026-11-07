cmake_minimum_required(VERSION 3.10)
project(cookie_clicker_cpp VERSION 2.021 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Required: SDL2 (window / input / timer / audio device). Present on this box.
# ---------------------------------------------------------------------------
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)

# ---------------------------------------------------------------------------
# Optional: SDL2_mixer (audio). If missing, Audio.cpp stub is used.
# ---------------------------------------------------------------------------
set(USE_SDL2_MIXER OFF CACHE BOOL "Use SDL2_mixer for audio")
if(USE_SDL2_MIXER)
    find_package(SDL2_mixer)
    if(SDL2_mixer_FOUND)
        set(USE_SDL2_MIXER ON)
    endif()
endif()

# ---------------------------------------------------------------------------
# Optional: Dear ImGui (immediate-mode UI). Expects third_party/imgui or a
# CMake-provided IMGUI_DIR. If missing, Render.cpp stub is used.
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imgui" CACHE PATH "Dear ImGui root")
set(USE_IMGUI OFF)
if(EXISTS "${IMGUI_DIR}/imgui.cpp")
    set(USE_IMGUI ON)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SOURCES
    main.cpp
    Types.cpp
    Game.cpp
    Economy.cpp
    Building.cpp
    Upgrade.cpp
    Achievement.cpp
    Buff.cpp
    Shimmer.cpp
    Particle.cpp
    Minigame.cpp
    Garden.cpp
    Grimoire.cpp
    Pantheon.cpp
    Dungeons.cpp
    DungeonGen.cpp
    SaveLoad.cpp
    Assets.cpp
    Audio.cpp
    Render.cpp
    EmbeddedAssets.cpp
)

set(HEADERS
    Types.h
    Game.h
    Economy.h
    Building.h
    Upgrade.h
    Achievement.h
    Buff.h
    Shimmer.h
    Particle.h
    Minigame.h
    Garden.h
    Grimoire.h
    Pantheon.h
    Dungeons.h
    DungeonGen.h
    SaveLoad.h
    Assets.h
    Audio.h
    Render.h
    EmbeddedAssets.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
if(USE_IMGUI)
    include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)
    set(SOURCES ${SOURCES}
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_executable(cookie_clicker ${SOURCES} ${HEADERS})

if(USE_IMGUI)
    target_compile_definitions(cookie_clicker PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
endif()

target_link_libraries(cookie_clicker PRIVATE SDL2::SDL2 OpenGL::GL
                                                    ${PNG_LIBRARY} ${FREETYPE_LIBRARIES})
target_include_directories(cookie_clicker PRIVATE ${PNG_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})

if(USE_SDL2_MIXER)
    target_link_libraries(cookie_clicker PRIVATE SDL2_mixer::SDL2_mixer)
endif()

if(APPLE)
    find_library(COCOA Cocoa)
    find_library(IOKIT IOKit)
    if(COCOA)
        target_link_libraries(cookie_clicker PRIVATE ${COCOA})
    endif()
    if(IOKIT)
        target_link_libraries(cookie_clicker PRIVATE ${IOKIT})
    endif()
endif()

# ---------------------------------------------------------------------------
# Build notes
# ---------------------------------------------------------------------------
message(STATUS "cookie_clicker_cpp: SDL2=YES SDL2_mixer=${USE_SDL2_MIXER} ImGui=${USE_IMGUI}")
if(NOT USE_IMGUI)
    message(WARNING "third_party/imgui not found at ${IMGUI_DIR}; rendering is a stub.")
endif()
