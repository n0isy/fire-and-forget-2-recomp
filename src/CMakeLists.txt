cmake_minimum_required(VERSION 3.16)
project(ff2_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -O2)

find_package(SDL2 REQUIRED)

set(FF2_CORE
    platform/platform_sdl.c
    asset/depack.c
    asset/ilbm.c
    render/bob.c
)

# skeleton (EGA palette test pattern)
add_executable(ff2 main.c platform/platform_sdl.c)
target_include_directories(ff2 PRIVATE ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ff2 PRIVATE ${SDL2_LIBRARIES})

# vertical-slice demo (live asset pipeline)
add_executable(ff2demo demo.c ${FF2_CORE})
target_include_directories(ff2demo PRIVATE ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ff2demo PRIVATE ${SDL2_LIBRARIES})

# playable level build (gameplay port over the foundation)
add_executable(ff2game
    game_app.c
    game/game_main.c
    game/dgroup.c
    game/entity_dispatch.c
    game/entity_faithful.c
    game/behaviors.c
    game/vm.c
    game/player_render.c
    game/render_world.c
    game/run_level.c
    game/keyboard.c
    game/camera_faithful.c
    game/bg_layers.c
    game/panel_text.c
    game/sprite_dl.c
    game/hud_cockpit.c
    game/display_init.c
    game/frontend.c
    game/cutscene.c
    game/decor_ring.c
    game/shape_gen.c
    game/path_vm.c
    game/data/wave_data.c
    game/data/enemies.c
    game/data/shapes.c
    game/data/road.c
    game/data/ui.c
    game/data/misc.c
    render/font.c
    ${FF2_CORE}
)
target_include_directories(ff2game PRIVATE ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ff2game PRIVATE ${SDL2_LIBRARIES})
