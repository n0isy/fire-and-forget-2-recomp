#!/bin/bash
# build_wasm.sh — compile the game to WebAssembly: emits web/index.{html,js,wasm,data}.
# Serve the web/ directory with any static file server and open index.html.
#
# Requires the emscripten SDK (emcc on PATH, or set EMSDK to the SDK root) and
# an assets/ directory built from your own game files (scripts/extract_assets.sh).
set -e
if ! command -v emcc >/dev/null 2>&1; then
    EMSDK="${EMSDK:-/opt/emsdk}"
    [ -f "$EMSDK/emsdk_env.sh" ] || { echo "emcc not found; install emscripten or set EMSDK" >&2; exit 1; }
    . "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
fi

SRC="game_app.c \
game/game_main.c game/dgroup.c game/entity_dispatch.c game/entity_faithful.c game/behaviors.c \
game/vm.c game/player_render.c game/render_world.c game/run_level.c game/keyboard.c game/camera_faithful.c \
game/bg_layers.c game/panel_text.c game/sprite_dl.c game/hud_cockpit.c game/display_init.c \
game/frontend.c game/cutscene.c game/decor_ring.c game/shape_gen.c game/path_vm.c \
render/font.c platform/platform_sdl.c asset/depack.c asset/ilbm.c render/bob.c"

cd "$(dirname "$0")/src"
emcc $SRC -I . -O2 -std=c11 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 \
     --preload-file ../assets@assets \
     --shell-file ../web/shell.html \
     -o ../web/index.html
echo "WASM built -> web/index.{html,js,wasm,data}"
