#!/bin/bash
# build_wasm.sh — compile the playable level build (ff2game) to WebAssembly and
# emit web/index.{html,js,wasm,data}. Serve web/ with any static file server;
# the reference deployment is https://titustribute.com/ff2/ (in the dev sandbox
# web/ is bind-mounted into a caddy container, so rebuilding updates the site
# instantly — hard-refresh the browser: file names are stable).
# Sources mirror the ff2game target in src/CMakeLists.txt; SDL2 comes from
# emscripten's -sUSE_SDL=2, and the game's assets/ dir is packaged with
# --preload-file (mounts at "assets", matching FF2_ASSETS=assets / getenv default).
#
# Requires the emscripten SDK (here at /opt/emsdk). Run from the repo root.
set -e
. /opt/emsdk/emsdk_env.sh >/dev/null 2>&1

SRC="game_app.c \
game/game_main.c game/dgroup.c game/entity_dispatch.c game/entity_faithful.c game/behaviors.c \
game/vm.c game/player_render.c game/render_world.c game/run_level.c game/keyboard.c game/camera_faithful.c \
game/bg_layers.c game/panel_text.c game/sprite_dl.c game/hud_cockpit.c game/display_init.c \
game/frontend.c game/cutscene.c game/decor_ring.c game/shape_gen.c game/path_vm.c \
game/sound.c game/data/sound.c platform/opl.c third_party/nuked_opl3/opl3.c \
game/data/wave_data.c game/data/enemies.c game/data/shapes.c game/data/road.c \
game/data/ui.c game/data/misc.c \
render/font.c platform/platform_sdl.c asset/depack.c asset/ilbm.c render/bob.c"

cd "$(dirname "$0")/src"
emcc $SRC -I . -O2 -std=c11 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 \
     --preload-file ../assets@assets --exclude-file "*dgroup.bin" \
     --shell-file ../web/shell.html \
     -o ../web/index.html
echo "WASM built -> web/index.{html,js,wasm,data}  (live: https://titustribute.com/ff2/)"
