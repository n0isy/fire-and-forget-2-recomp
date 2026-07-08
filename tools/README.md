# tools/ — extraction & reconstruction

Utilities that pull data out of **your own** copy of the original game and recover the
in-house level scripts. None of them are needed to *build or run* the engine (the
game-logic data already ships as C source under `src/game/data/`); they are the
provenance of that data and the way to regenerate it.

## Level / wave scripting (the recovered DSL)

The original game embedded an in-house script VM in its DGROUP data segment. Its keyword
table (`CLEAR GETCHAR PUT WAVE GOSUB RETURN BEQ WHILE GOTO SETDIST …`) survives in the
blob; these two tools recover and round-trip the language:

| tool | does |
|---|---|
| `wavedis.py` | **losslessly** disassembles the original DGROUP wave bytecode → `wave/levels.wave` (mnemonics, enemy-name symbols, labels, cut/dead code kept verbatim as `DEFB`) |
| `waveasm.py` | assembles `wave/levels.wave` back to bytes; `--verify` memcmp's against the blob; `--c src/game/data/wave_data.c` emits the generated C the engine executes |

Round trip is byte-exact: all 3090 bytecode bytes + the 6-entry level directory reassemble
identically to the original.

```sh
# edit levels, then regenerate the committed bytecode:
tools/waveasm.py wave/levels.wave --c src/game/data/wave_data.c
```

## CPT graphics decoders

Standalone decoders for inspecting your copy's graphics (the engine also decodes CPT at
runtime, and `scripts/extract_assets.sh` is what actually populates `assets/`).

| tool | does |
|---|---|
| `pp20_unpack.py` | PowerPacker PP20 decruncher for the `*.CPT` assets |
| `bob_extract.py` | decodes the BOB.CPT sprite bank |

> These tools operate on **your own** copy of the original game. No original binaries,
> graphics or the DGROUP blob are committed to this repository.
