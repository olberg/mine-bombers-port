# Unity test framework (ThrowTheSwitch)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# Unity doesn't provide a CMake target, so create one
add_library(unity_lib STATIC
    ${unity_SOURCE_DIR}/src/unity.c
)
target_include_directories(unity_lib PUBLIC ${unity_SOURCE_DIR}/src)

# Helper function to add a test (extra args = additional source files)
function(add_unity_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE unity_lib raylib)
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    # Copy assets so tests can load them (skipped when the original game
    # data is absent; see README.md for supplying your own copy)
    if(EXISTS ${CMAKE_SOURCE_DIR}/original)
        add_custom_command(TARGET ${TEST_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/original
            $<TARGET_FILE_DIR:${TEST_NAME}>/assets
            COMMENT "Copying assets for ${TEST_NAME}"
        )
    endif()
endfunction()

# Example test to verify the framework works
add_unity_test(test_example test_example.c)

# PRNG test (Borland Pascal 7 LCG characterization)
add_unity_test(test_prng test_prng.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# SPY loader test
add_unity_test(test_spy_loader test_spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Palette test
add_unity_test(test_palette test_palette.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c)

# Font loader test
add_unity_test(test_font_loader test_font_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c)

# Sprite sheet test
add_unity_test(test_sprite_sheet test_sprite_sheet.c
    ${CMAKE_SOURCE_DIR}/src/loaders/sprite_sheet.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Config test
add_unity_test(test_config test_config.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c)

# VOC loader test
add_unity_test(test_voc_loader test_voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c)

# Map loader test
add_unity_test(test_map_loader test_map_loader.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Player test
add_unity_test(test_player test_player.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c)

# Weapons test
add_unity_test(test_weapons test_weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c)

# Player database test
add_unity_test(test_player_db test_player_db.c
    ${CMAKE_SOURCE_DIR}/src/game/player_db.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c)

# PCX loader test
add_unity_test(test_pcx_loader test_pcx_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/pcx_loader.c)

# Player input test
add_unity_test(test_player_input test_player_input.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c)

# Sprites test
add_unity_test(test_sprites test_sprites.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Map renderer test
add_unity_test(test_map_renderer test_map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Movement test
add_unity_test(test_movement test_movement.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Entity test
add_unity_test(test_entity test_entity.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# AI test
add_unity_test(test_ai test_ai.c
    ${CMAKE_SOURCE_DIR}/src/game/ai.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Bombs test
add_unity_test(test_bombs test_bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# HUD test
add_unity_test(test_hud test_hud.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c)

# Shop test
add_unity_test(test_shop test_shop.c
    ${CMAKE_SOURCE_DIR}/src/game/shop.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Player select test
add_unity_test(test_player_select test_player_select.c
    ${CMAKE_SOURCE_DIR}/src/game/player_select.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/player_db.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/sprite_sheet.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/pcx_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c)

# Round test
add_unity_test(test_round test_round.c
    ${CMAKE_SOURCE_DIR}/src/game/round.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/ai.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Map list test
add_unity_test(test_map_list test_map_list.c
    ${CMAKE_SOURCE_DIR}/src/game/map_list.c)

# Shop layout analysis test
add_unity_test(test_shop_layout test_shop_layout.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Results screen ranking test (links the real results.c)
add_unity_test(test_results test_results.c
    ${CMAKE_SOURCE_DIR}/src/game/results.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/player_db.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/pcx_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c)

# Random map generation test
add_unity_test(test_random_map test_random_map.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Explosion damage test
add_unity_test(test_explosion_damage test_explosion_damage.c
    ${CMAKE_SOURCE_DIR}/src/game/bombs.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/movement.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/map.c
    ${CMAKE_SOURCE_DIR}/src/game/entity.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/audio/sfx.c
    ${CMAKE_SOURCE_DIR}/src/loaders/voc_loader.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)

# Visibility / fog-of-war test
add_unity_test(test_visibility test_visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/visibility.c
    ${CMAKE_SOURCE_DIR}/src/game/map_renderer.c
    ${CMAKE_SOURCE_DIR}/src/game/sprites.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c)

# Sound config test
add_unity_test(test_sound_config test_sound_config.c
    ${CMAKE_SOURCE_DIR}/src/game/sound_config.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c)

# Music schedule test (original song-position scheduling)
add_unity_test(test_music_schedule test_music_schedule.c
    ${CMAKE_SOURCE_DIR}/src/game/music_schedule.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)
target_link_libraries(test_music_schedule PRIVATE xmp_static)

# Seeded full-match soak: runs the real game exe in autoplay
# mode (2x 5-round determinism check + 25-round completion). RUN_SERIAL:
# it opens a window and runs uncapped; parallel ctest would skew nothing
# logically but fights for the GPU/foreground.
add_test(NAME test_mp_soak
    COMMAND ${CMAKE_COMMAND}
        -DGAME_EXE=$<TARGET_FILE:minebombers>
        -DWORK_DIR=$<TARGET_FILE_DIR:minebombers>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_mp_soak.cmake)
set_tests_properties(test_mp_soak PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)

# Map picker test
add_unity_test(test_map_picker test_map_picker.c
    ${CMAKE_SOURCE_DIR}/src/game/map_picker.c
    ${CMAKE_SOURCE_DIR}/src/game/map_list.c
    ${CMAKE_SOURCE_DIR}/src/loaders/spy_loader.c
    ${CMAKE_SOURCE_DIR}/src/loaders/font_loader.c
    ${CMAKE_SOURCE_DIR}/src/gfx/palette.c
    ${CMAKE_SOURCE_DIR}/src/game/config.c
    ${CMAKE_SOURCE_DIR}/src/input/input.c
    ${CMAKE_SOURCE_DIR}/src/util/prng.c)
