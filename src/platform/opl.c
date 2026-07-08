/* opl.c — the OPL2 SYNTH presentation layer (native SDL2 + emscripten).
 *
 * The game's ported AdLib driver (game/sound.c) emits the faithful OPL
 * register write stream through ff_opl_out(reg, val) — THE acceptance-tested
 * output (bit-compared against the original's port 0x388/0x389 writes). This
 * file turns that stream into audio with the vendored Nuked-OPL3 core
 * (src/third_party/nuked_opl3, LGPL-2.1 — cycle-accurate YMF262; OPL2
 * programs run in its native compatibility mode).
 *
 * TIMING: each write is stamped with the driver's music-tick counter
 * (91.03 Hz = PIT reload 0x3333). The audio callback replays events on a
 * sample-domain clock (SAMPLES_PER_TICK = RATE * 13107 / 1193182) through an
 * SPSC ring; the game thread produces (it is wall-paced at 18.2 fps by
 * ff_wait_frame / the emscripten main loop), the audio thread consumes, and
 * large drifts (pause, debugger) snap the playhead. Under emscripten the
 * "callback" runs on the main thread via WebAudio, so the ring is trivially
 * single-threaded there; native uses SDL's audio thread (C11 atomics).
 */
#include <SDL2/SDL.h>
#include <stdatomic.h>
#include "../third_party/nuked_opl3/opl3.h"
#include "../ff2_types.h"

u32  snd_total_ticks(void);          /* game/sound.c — the music tick counter */

#define OPL_RATE   48000
#define RING_SIZE  8192              /* power of two                          */

typedef struct { u32 tick; u8 reg, val; } OplEv;
static OplEv        s_ring[RING_SIZE];
static _Atomic u32  s_head;          /* producer (game thread)                */
static _Atomic u32  s_tail;          /* consumer (audio thread)               */

static opl3_chip s_chip;
static SDL_AudioDeviceID s_dev;
static double s_gen_t;               /* playhead in ticks (audio thread only) */
static double s_tps;                 /* ticks per sample (set from the REAL
                                        device rate — the browser may give
                                        44100 instead of the requested 48000) */

/* the STRONG sink overriding the weak no-op in game/sound.c */
void ff_opl_out(u8 reg, u8 val)
{
    if (!s_dev) return;
    u32 h = atomic_load_explicit(&s_head, memory_order_relaxed);
    u32 t = atomic_load_explicit(&s_tail, memory_order_acquire);
    if (h - t >= RING_SIZE) return;              /* full: drop (never in play) */
    s_ring[h & (RING_SIZE - 1)] = (OplEv){ snd_total_ticks(), reg, val };
    atomic_store_explicit(&s_head, h + 1, memory_order_release);
}

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    i16 *out = (i16 *)stream;
    int frames = len / (int)(2 * sizeof(i16));   /* stereo S16 */
    while (frames > 0) {
        u32 h = atomic_load_explicit(&s_head, memory_order_acquire);
        u32 t = atomic_load_explicit(&s_tail, memory_order_relaxed);
        int gen = frames;
        if (h != t) {
            OplEv ev = s_ring[t & (RING_SIZE - 1)];
            double dt = (double)ev.tick - s_gen_t;       /* ticks until the event */
            if (dt > 91.0 * 2.0) s_gen_t = (double)ev.tick - 9.0;   /* resync: >2 s ahead */
            if (dt <= 0.0) {                             /* due: apply now        */
                OPL3_WriteRegBuffered(&s_chip, ev.reg, ev.val);
                atomic_store_explicit(&s_tail, t + 1, memory_order_release);
                continue;
            }
            int till = (int)(dt / s_tps) + 1;
            if (till < gen) gen = till;
        }
        OPL3_GenerateStream(&s_chip, out, (u32)gen);
        out += gen * 2;
        frames -= gen;
        s_gen_t += (double)gen * s_tps;
    }
}

/* open the audio device; safe to call when audio is unavailable (headless). */
void ff_sound_start(void)
{
    if (s_dev) return;
    if (!(SDL_WasInit(0) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = OPL_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = audio_cb;
    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!s_dev) return;
    OPL3_Reset(&s_chip, (u32)have.freq);       /* the REAL device rate (the
                                                * browser often gives 44100) */
    s_tps   = 1193182.0 / 13107.0 / (double)have.freq;
    s_gen_t = (double)snd_total_ticks();
    SDL_PauseAudioDevice(s_dev, 0);
}
