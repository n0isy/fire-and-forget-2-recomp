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

## You need the original game's graphics

The reconstructed engine **and all of its game‑logic data** (the level scripts, enemy
tables, palettes, the sprite‑composer programs, LUTs, texts …) ship as readable source,
so you no longer need the original executable's DGROUP blob to build or run — the build
is self‑contained (see *Reconstructed data & tooling* below).

What is **not** reconstructed is the original **graphics**. To play you must own
Fire & Forget II and extract its screens, sprites and HUD font from your own copy:

```sh
scripts/extract_assets.sh /path/to/your/FF2      # needs FF2EGA.EXE, *.CPT, HIGH
```

This fills the (never‑committed) `assets/` directory with your copy's `cpt/*.CPT`
graphics, the `font_ega.bin` HUD font and your `HIGH` score file. It also cuts
`dgroup.bin`, which the engine no longer reads — it only lets `tools/waveasm.py --verify`
cross‑check the shipped level scripts against your own original.

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
src/game/data/  the DGROUP game‑logic data, reconstructed as typed C source
src/CMakeLists.txt
wave/           level scripts in the recovered wave DSL (source of wave_data.c)
tools/          reconstruction tooling (wave (dis)assembler, extractors, Ghidra scripts)
web/            browser shell (HTML/CSS/JS + vendored nipplejs for the touch joystick)
scripts/        graphics extraction from your own game copy
build_wasm.sh   WebAssembly build
assets/         your copy's graphics, created by scripts/extract_assets.sh (never committed)
```

Source files carry detailed provenance comments: each routine references the original
function it reconstructs (segment:offset addresses) and the evidence used.

## Reconstructed data & tooling

The original kept its level scripts and lookup tables in the executable's DGROUP data
segment. Instead of loading that binary blob at runtime, this project reconstructs it as
readable, **typed C source** under `src/game/data/` — enemy prototypes, the morph and
sprite‑composer programs, palettes, panel texts, the road curve, the sine/perspective
LUTs, the RNG seed, the demo input tape, and so on. That is what makes the build
self‑contained.

The most interesting piece is the **level/wave scripting**. The original game shipped an
in‑house script VM — its keyword table (`CLEAR`, `GETCHAR`, `PUT`, `WAVE`, `GOSUB`, …)
still sits in the data segment. We recovered that little language:

- `tools/wavedis.py` **losslessly disassembles** the original DGROUP bytecode into the
  human‑readable DSL at [`wave/levels.wave`](wave/levels.wave) — mnemonics, enemy‑name
  symbols, labels, even the cut/dead scripts preserved verbatim as `DEFB`;
- `tools/waveasm.py` **assembles** `wave/levels.wave` back to bytes and emits the
  generated `src/game/data/wave_data.c` that the engine executes. The round trip is
  **byte‑verified** against the original — all 3090 bytecode bytes plus the level
  directory reassemble identically.

So `wave/levels.wave` is the editable source of truth for the level design, and the
committed `wave_data.c` is its build product (regenerate with
`tools/waveasm.py wave/levels.wave --c src/game/data/wave_data.c`). The other tools in
`tools/` are `pp20_unpack.py` / `bob_extract.py`, standalone decoders for the CPT
graphics. See [`tools/README.md`](tools/README.md).

## Legal

This is an unofficial fan preservation / research project, not affiliated with or endorsed
by the rights holders. *Fire & Forget II* and all original game content are the property of
their respective owners. This repository ships **no original binaries or graphics** — no
executable, no CPT screens/sprites, no fonts; you must own the original game and extract
those from your own copy to play. The engine and its data tables are an original
transliteration of the disassembled game, provided for educational and preservation purposes.

[nipplejs](https://github.com/yoannmoinet/nipplejs) is MIT-licensed.
