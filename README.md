# Fire & Forget II — Recomp

A faithful modern reconstruction (“recompilation”) of **Fire & Forget II: The Death Convoy**
(Titus Interactive, 1990, DOS/EGA) as portable **C11 + SDL2**, playable natively and in the
browser via **WebAssembly** — including touch controls for mobile.

The engine was reconstructed by reading the disassembled/decompiled original DOS executable
and transliterating it function by function — *not* by approximating gameplay from videos.
Every reconstructed subsystem was verified against the real game running under QEMU + GDB:

- the full 2041-frame attract demo is **bit-exact** in game state, entity pool and roadside
  décor on every frame, and pixel-exact on every sampled screen;
- an end-to-end run (boot → menu → race → game over) compares **pixel-identical on all
  3161 frames**, whole screen, no masks;
- a 1.5-level play-through from a recorded input tape is pixel-identical on 4088 of 4095
  frames (the rest is one known sub-10-pixel horizon detail under investigation);
- all 81 enemy behaviour handlers, the level script VM, the runtime sprite composer, the
  takeoff/flight/landing modes, stages 1–5 content, the nuclear cutscene, the highscore
  name entry and the arcade attract loop are ported and bit-verified — even two genuine
  ROM quirks of the original blitter are reproduced.

**Not ported yet:** sound (AdLib / PC speaker) — the only missing subsystem.

## You need the original game files

No game data ships with this repository. To build a playable game you must own
Fire & Forget II and point the extraction script at your game directory
(it needs `FF2EGA.EXE`, the `*.CPT` files and `HIGH`):

```sh
scripts/extract_assets.sh /path/to/your/FF2
```

This produces the `assets/` directory the engine loads (two data blobs cut from your
executable + your CPT screens/sprites + your highscore file).

## Build & run

Native (needs SDL2 + CMake):

```sh
cmake -S src -B build
cmake --build build
FF2_ASSETS=assets ./build/ff2game
```

Browser (needs the [emscripten SDK](https://emscripten.org)):

```sh
./build_wasm.sh          # emits web/index.{html,js,wasm,data}
# serve web/ with any static file server and open index.html
```

## Controls

| Action | Keyboard | Mobile |
|---|---|---|
| Steer / gas / brake | `←` `→` / `↑` / `↓` | virtual joystick (left) |
| **FIRE** — machine gun; in the menu: insert coin | `Space` or `Ctrl` | 🔫 button |
| **START·FLY** — start; at full throttle: take off / land | `Enter` or `Shift` | ▲ button |
| **MISSILE** — FIRE+START together (uses fuel) | chord | 🚀 button |
| Restart (web) / quit (native) | `Esc` | — |

On touch devices the on-screen controls appear automatically (append `?touch` to the URL
to force them). A fullscreen button sits in the top-right corner.

## Project layout

```
src/            the reconstructed engine (C11), one module per original subsystem
src/CMakeLists.txt
web/            browser shell (HTML/CSS/JS + vendored nipplejs for the touch joystick)
scripts/        asset extraction from your own game copy
build_wasm.sh   WebAssembly build
assets/         created by scripts/extract_assets.sh (never committed)
```

Source files carry detailed provenance comments: each routine references the original
function it reconstructs (segment:offset addresses) and the evidence used.

## Legal

This is an unofficial fan preservation / research project, not affiliated with or endorsed
by the rights holders. *Fire & Forget II* and all original game content are the property of
their respective owners. The repository contains no original game assets, code or data;
you must own the original game to play. The reconstructed source is provided for
educational and preservation purposes.

[nipplejs](https://github.com/yoannmoinet/nipplejs) is MIT-licensed.
