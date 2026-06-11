cmake_minimum_required(VERSION 3.20)
project(minebombers C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

# Raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# libxmp (S3M module playback)
FetchContent_Declare(
    libxmp
    GIT_REPOSITORY https://github.com/libxmp/libxmp.git
    GIT_TAG        libxmp-4.6.0
    GIT_SHALLOW    TRUE
)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
# libxmp 4.6.0 declares a pre-3.5 CMake minimum, which CMake 4+ refuses to
# configure; this opts back in without touching the pinned libxmp version.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(libxmp)

# Game executable
add_executable(minebombers
    src/main.c
    src/debug_overlay.c
    src/autoplay.c
    src/util/prng.c
    src/util/asset_check.c
    src/loaders/spy_loader.c
    src/loaders/font_loader.c
    src/loaders/sprite_sheet.c
    src/loaders/voc_loader.c
    src/gfx/palette.c
    src/audio/music.c
    src/audio/sfx.c
    src/input/input.c
    src/game/title.c
    src/game/menu.c
    src/game/config.c
    src/game/options.c
    src/game/info.c
    src/game/map.c
    src/game/map_list.c
    src/game/player.c
    src/game/player_db.c
    src/game/weapons.c
    src/game/sprites.c
    src/game/map_renderer.c
    src/game/movement.c
    src/game/bombs.c
    src/game/entity.c
    src/game/ai.c
    src/game/hud.c
    src/game/shop.c
    src/game/player_select.c
    src/game/round.c
    src/game/visibility.c
    src/game/results.c
    src/game/hall_of_fame.c
    src/game/sp_complete.c
    src/game/sp_level_complete.c
    src/game/key_config.c
    src/game/sound_config.c
    src/game/map_picker.c
    src/game/music_schedule.c
    src/loaders/pcx_loader.c
)
target_link_libraries(minebombers PRIVATE raylib xmp_static)
target_include_directories(minebombers PRIVATE src)

# Copy assets to the build directory when the original game data is present.
# The original game is not distributed with this repository; see README.md
# for how to supply your own copy into the assets/ directory next to the exe.
if(EXISTS ${CMAKE_SOURCE_DIR}/original)
    add_custom_command(TARGET minebombers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/original
        $<TARGET_FILE_DIR:minebombers>/assets
        COMMENT "Copying game assets"
    )
endif()

# Tests
enable_testing()
add_subdirectory(tests)
