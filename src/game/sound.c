/* sound.c — FAITHFUL port of the AdLib/OPL2 sound driver (segment 143A):
 *
 *   snd_init         sound_init fn143A_0349 (AdLib branch; the hardware probe
 *                    fn143A_04AB is a hardware side effect, omitted — the port
 *                    always "has" an OPL2, exactly like the QEMU rig with the
 *                    detect forced; see qemu/chk_adlib2.py)
 *   snd_load_song    music_load_song fn143A_0429
 *   snd_sfx_trigger  fn143A_05B7 (AdLib branch: replace the song/SFX stack)
 *   snd_sfx_queue    fn143A_0643 (AdLib branch: push onto the stack)
 *   snd_sfx_play     fn143A_027B (AdLib branch: percussion one-shots, gated by
 *                    the F3 toggle b40A0 — OFF by default, like the original)
 *   snd_frame        5 music-ISR ticks (the 143A:08B5 body from 0x8E3 on).
 *
 * TIMING MODEL. The original hooks INT8 at PIT reload 0x3333 (91.03 Hz) and
 * derives the frame rate from it (/5 -> bBCBB 18.2 fps -> /18 -> t24F1), so a
 * game frame contains EXACTLY 5 music ticks, phase-locked to the frame
 * boundary (the boundary IS the bcba-wrap tick — no free phase constant).
 * Under QEMU TCG the frame's game logic executes in ~zero virtual time right
 * after the boundary tick, so every game-code sound call lands BEFORE the
 * frame's first music tick. The port therefore runs snd_frame() at the END of
 * each presented frame, after all logic (triggers) — the same interleave.
 * (The bcba/bcbb/t24F1 divider chain itself is NOT here: run_level.c /
 * frontend.c already model it via s_isr18/fe_isr18.)
 *
 * OUTPUT = the OPL REGISTER WRITE STREAM through opl_write() (fn143A_0D18
 * gated by the F2 mute toggle b409F / fn143A_0D38 forced). The acceptance
 * comparison is this stream + the per-frame voice state, not audio samples;
 * the synth (platform ff_opl_out) is a presentation layer.
 *
 * SOURCE OF TRUTH: ndisasm of 143A:08B5..0D51 (/tmp/isr.asm, EXE file offset
 * 0xDA55; Reko missed the ISR entirely) cross-checked with docs/sound_format.md
 * and raw/reko/FF2EGA_143A.c for the C-shaped callers. Every constant, table
 * offset, flag store and register-write ORDER is transliterated.
 *
 * DATA (data/sound.c, byte-exact from DGROUP): streams @0x7F69, song table
 * @0xCFCC, percussion ptrs @0xD046, instrument ptrs @0xD050, song dirs
 * @0xD074, slot tables @0xD1BA/0xD1CD, F-numbers @0xD1D8, patches @0xD1F6.
 * All pointer values keep the ORIGINAL DGROUP OFFSETS as opaque keys.
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the de-DGROUP'd sound state (original homes in comments) ------------ */
typedef struct {            /* 16-byte song-header stack slot @0xD15C + n*0x10 */
    u16 cursor;             /* +0: working voice cursor (transient)            */
    u16 instr;              /* +2: instrument ptr table (DGROUP offset)        */
    u8  bd;                 /* +4: OPL reg 0xBD shadow (blob init 0x20)        */
    u8  tempo_reload;       /* +8 */
    u8  tempo_ctr;          /* +9 */
} SndHdr;

static SndHdr     s_hdr[3];             /* @0xD15C/6C/7C                      */
static AdlibVoice s_voice[3][10];       /* voice arrays @0xD2E0/0xD394/0xD448 */
static u16 s_stack;                     /* ptrBD8E: 0 / 0x10 / 0x20           */
static u16 s_d1f2;                      /* active-voice count @0xD1F2         */
static u8  s_d1f4;                      /* re-arm patches flag @0xD1F4        */
static u8  s_d1f5;                      /* new-song key-off flag @0xD1F5      */
static u16 s_d044;                      /* percussion one-shot ptr+1 @0xD044  */
static u8  s_bd8d;                      /* busy flag bBD8D (blocks spk gate)  */
static u8  s_b409f;                     /* F2 toggle @0x409F: OPL mute        */
static u8  s_b40a0;                     /* F3 toggle @0x40A0: one-shots enable*/

#define HDR   (&s_hdr[s_stack >> 4])
#define VOICE (s_voice[s_stack >> 4])

/* ---- OPL register write log + sink ---------------------------------------- */
static FILE *s_log;
static u32   s_frame;      /* presented-frame counter (snd_frame() increments) */
static u32   s_seq;        /* per-frame write sequence                         */
static u32   s_ticks;      /* total music ticks (the synth's time base)        */

void snd_set_log(void *f) { s_log = (FILE *)f; s_seq = 0; }
u32  snd_frame_no(void)   { return s_frame; }
u32  snd_total_ticks(void){ return s_ticks; }
int  snd_muted(void)      { return s_b409f != 0; }

/* platform synth sink (step 5: Nuked-OPL3 behind SDL audio); weak no-op so the
 * headless builds run without a synth. */
__attribute__((weak)) void ff_opl_out(u8 r, u8 v) { (void)r; (void)v; }

/* fn143A_0D18 (gated on the F2 mute b409F) / fn143A_0D38 (forced) */
static void opl_write(u8 reg, u8 val)
{
    if (s_b409f != 0) return;
    if (s_log) fprintf(s_log, "W,%u,%u,G,%02X,%02X\n", s_frame, s_seq++, reg, val);
    ff_opl_out(reg, val);
}
static void opl_write_forced(u8 reg, u8 val)
{
    if (s_log) fprintf(s_log, "W,%u,%u,F,%02X,%02X\n", s_frame, s_seq++, reg, val);
    ff_opl_out(reg, val);
}

/* ---- data accessors (original DGROUP offsets as keys) --------------------- */
static u8  stream_b(u16 off) { return ffd_song_streams[off - 0x7F69]; }
static u16 stream_w(u16 off) { return (u16)(stream_b(off) | ((u16)stream_b(off + 1) << 8)); }
static u16 dir_w(u16 off)    { u16 r = (u16)(off - 0xD074);
                               return (u16)(ffd_song_dirs[r] | ((u16)ffd_song_dirs[r + 1] << 8)); }
/* patch record byte @DGROUP offset (region 0xD1F6..0xD2E0) */
static u8  patch_b(u16 off)  { return (off >= 0xD1F6 && off < 0xD2E0)
                                      ? ffd_instr_patches[off - 0xD1F6] : 0; }
/* instrument table word: [instr_off + note*2]; instr_off is 0xD050 for every
 * song record (the table region 0xD050..0xD074). */
static u16 instr_w(u16 instr_off, u8 note)
{
    return ffd_instr_ptrs[((u16)(instr_off - 0xD050) >> 1) + note];
}
/* fnum fetch — FAITHFUL SPILL: the ISR indexes the 12-entry table @0xD1D8 with
 * the FULL 4-bit note nibble; indices 12..14 read the zeroed voice-flags words
 * @0xD1F0 (= 0 -> the rest/hold path), index 15 reads the word @0xD1F6 (the
 * unreferenced patch-region head = 0xF606). */
static u16 snd_fnum(u8 idx)
{
    if (idx < 12) return ffd_snd_fnum[idx];
    if (idx < 15) return 0;
    return (u16)(patch_b(0xD1F6) | ((u16)patch_b(0xD1F7) << 8));
}

/* ---- fn143A_0482 adlib_silence_all -----------------------------------------
 * BOTH loop bounds are 0xB9 in the ROM (`cmp al,0xb9; jc` — reko's `< ~0x46`;
 * ndisasm @143A:0919 confirms the ISR's inline copy too): the second loop
 * therefore writes A0..B8 (25 regs, spilling past A8 into B0..B8 again) —
 * a genuine ROM quirk, reproduced for register-stream identity. */
static void silence_all(void)
{
    for (u8 r = 0xB0; r < 0xB9; ++r) opl_write(r, 0x00);
    for (u8 r = 0xA0; r < 0xB9; ++r) opl_write(r, 0x00);
}

/* ---- write5 (fn143A_0CE6 / forced fn143A_0CB4): one operator's 5 regs from
 * a patch record at DGROUP offset `rec`, slot offset `dl`. ------------------ */
static void write5(u16 rec, u8 dl)
{
    opl_write((u8)(0x60 + dl), patch_b(rec));
    opl_write((u8)(0x80 + dl), patch_b(rec + 1));
    opl_write((u8)(0x40 + dl), patch_b(rec + 2));
    opl_write((u8)(0x20 + dl), patch_b(rec + 3));
    opl_write((u8)(0xE0 + dl), patch_b(rec + 4));
}
static void write5_forced(u16 rec, u8 dl)
{
    opl_write_forced((u8)(0x60 + dl), patch_b(rec));
    opl_write_forced((u8)(0x80 + dl), patch_b(rec + 1));
    opl_write_forced((u8)(0x40 + dl), patch_b(rec + 2));
    opl_write_forced((u8)(0x20 + dl), patch_b(rec + 3));
    opl_write_forced((u8)(0xE0 + dl), patch_b(rec + 4));
}

/* ---- cmd1 @143A:0B0E — set volume + write the CARRIER total level --------- */
static void set_volume(AdlibVoice *v, u8 n)
{
    v->volume = n;
    u8 al = (u8)(n + 1);
    u16 rec = instr_w(HDR->instr, v->cur_note);
    if (v->cur_note >= 0x0F) rec += 1;              /* skip the channel byte   */
    u16 cx = al;
    u8 ah = (u8)(patch_b(rec + 2) & 0x3F);          /* patch KSL|TL            */
    ah = (u8)(0x3F - ah);                           /* inverted level          */
    u16 ax = (u16)(((u16)ah << 8) >> 4);            /* ah<<4                   */
    ax = (u16)(ax * cx);                            /* low word of the product */
    u8 fin = (u8)(0x3F - (ax >> 8));                /* 0x3F - product hi byte  */
    u8 bl = ffd_opl_chan_cell[v->opl_channel];
    if (bl <= 0x0D) bl = (u8)(bl + 3);              /* carrier cell            */
    opl_write((u8)(0x40 + ffd_opl_cell_reg[bl]), fin);
}

/* ---- note_on / key state (cmd6 @143A:0B76; rekey body @0B7D) -------------- */
static void note_on(AdlibVoice *v, u8 n)
{
    v->keystate = n;
    if (n == 1) {                                   /* percussion key-on        */
        u8 bl = (u8)(v->block + 0x0F);              /* cur_note = block + 0xF   */
        v->cur_note = bl;
        u16 rec = instr_w(HDR->instr, bl);
        u8 ch = patch_b(rec);                       /* leading channel byte     */
        v->opl_channel = ch;
        u8 sh = (u8)(ch - 6);
        HDR->bd |= (u8)(sh < 8 ? (0x10 >> sh) : 0); /* set the drum key bit     */
    } else {
        if (n > 1) n -= 1;                          /* melodic instrument index */
        v->cur_note = n;
        u8 sh = (u8)(v->opl_channel - 6);
        if ((i8)(v->opl_channel) >= 6) {            /* ch >= 6: clear drum bit  */
            HDR->bd &= (u8)~(sh < 8 ? (0x10 >> sh) : 0);
            opl_write(0xBD, HDR->bd);
        }
    }
    /* load the patch (@0BC8): modulator slot; ch <= 6 uses cell+3 here */
    u8 bl = ffd_opl_chan_cell[v->opl_channel];
    if (v->opl_channel <= 6) bl = (u8)(bl + 3);
    u8 dl = ffd_opl_cell_reg[bl];
    u16 rec = instr_w(HDR->instr, v->cur_note);
    if (v->cur_note >= 0x0F) rec += 1;
    write5(rec, dl);
    dl = (u8)(dl - 3);
    rec += 5;
    if (v->opl_channel < 7) {                       /* melodic: op2 + feedback  */
        write5(rec, dl);
        opl_write((u8)(0xC0 + v->opl_channel), patch_b(rec + 5));
    }
}

/* ---- note_update @143A:0A4B (ax = fnum, cl = block nibble) ---------------- */
static void note_update(AdlibVoice *v, u16 fnum, u8 cl)
{
    if (fnum == 0) { v->prev_gate = v->gate; return; }      /* rest/hold        */
    if (v->cur_note >= 0x0F) {                              /* PERCUSSION voice */
        cl = (u8)(cl + 0x0F);
        if (v->cur_note != cl) {                            /* new drum: rekey  */
            note_on(v, 1);                                  /* @0B7D body       */
            set_volume(v, v->volume);
        }
        u8 sh = (u8)(v->opl_channel - 6);
        u8 ah = (u8)(~(sh < 8 ? (0x10 >> sh) : 0) & HDR->bd);
        opl_write(0xBD, ah);                                /* key the drum OFF */
        if (v->opl_channel == 6) {                          /* BD channel freq  */
            opl_write(0xA6, 0x57);
            opl_write(0xB6, 0x00);
            opl_write(0xB6, 0x05);
        }
        opl_write(0xBD, HDR->bd);                           /* key back ON      */
        v->prev_gate = v->gate;
    } else {                                                /* MELODIC voice    */
        cl = (u8)(cl + 2);                                  /* block += 2       */
        opl_write((u8)(0xA0 + v->opl_channel), (u8)fnum);
        if (v->note_lo != v->prev_note_lo || v->prev_gate == 0) {
            opl_write((u8)(0xB0 + v->opl_channel), 0x00);   /* key off first    */
            if (fnum == 0) { v->prev_gate = v->gate; return; }  /* (dead: fnum!=0) */
        }
        opl_write((u8)(0xB0 + v->opl_channel),
                  (u8)(0x20 | (u8)(cl << 2) | (u8)(fnum >> 8)));
        v->prev_gate = v->gate;
    }
}

/* ---- music_load_song fn143A_0429 ------------------------------------------ */
static void snd_load_song(u16 dir_off, u8 tempo, u16 instr_off)
{
    SndHdr *h = HDR;
    AdlibVoice *va = VOICE;
    h->tempo_reload = tempo;
    h->tempo_ctr    = tempo;
    h->instr        = instr_off;
    u16 si = dir_off;
    u8 ch = 0;
    u16 stream;
    do {                                    /* incl. the 0xFFFF terminator slot */
        AdlibVoice *v = &va[ch];
        memset(v, 0, sizeof *v);
        stream = dir_w(si);
        v->stream_ptr  = stream;
        v->opl_channel = ch;
        si += 2;
        ch += 1;
    } while (stream != 0xFFFF);
    s_d1f5 = 1;                             /* key-off-all on the next tick     */
    silence_all();                          /* fn143A_0482 (immediate)          */
}

/* ---- sfx_trigger fn143A_05B7 (AdLib branch; replace != 0 resets the stack) - */
void snd_sfx_trigger(int id, int replace)
{
    s_bd8d = 1;
    const SndSong *s = &ffd_song_table[id];
    if (replace) s_stack = 0;
    snd_load_song(s->dir, (u8)s->tempo, s->instr);
    s_bd8d = 0;
}

/* ---- sfx_queue fn143A_0643 (AdLib branch; push a stack slot) --------------- */
void snd_sfx_queue(int id)
{
    s_bd8d = 1;
    if (s_stack < 0x20) s_stack += 0x10;    /* 3 header slots exist (blob);
                                             * deeper pushes would corrupt in
                                             * the original — never happens */
    const SndSong *s = &ffd_song_table[id];
    snd_load_song(s->dir, (u8)s->tempo, s->instr);
    s_bd8d = 0;
}

/* ---- sfx_play fn143A_027B (AdLib branch: percussion one-shots) -------------
 * Gated by the F3 toggle b40A0 (OFF by default — the original enables the
 * one-shot/speaker effects only after an F3 keypress). ids 0 gun / 0x13
 * explosion / 0x14 missile; everything else is a no-op on AdLib. */
void snd_sfx_play(int id)
{
    if (s_b40a0 == 0) return;
    u16 ax;
    if (id == 0x00)      ax = 2;
    else if (id == 0x13) ax = 0;
    else if (id == 0x14) ax = 4;
    else return;
    s_d044 = (u16)(ffd_perc_ptrs[ax >> 1] + 1);   /* word @0xD046+ax, +1 skips
                                                   * the record's channel byte */
}

/* the F2/F3 sound toggles (INT9 `toggle[sc] ^= 0xFF` on key press) */
void snd_toggle_mute(void)    { s_b409f ^= 0xFF; }
void snd_toggle_oneshot(void) { s_b40a0 ^= 0xFF; }

/* ---- one music-ISR tick: the 143A:08B5 body from 0x8E3 --------------------- */
static void snd_tick(void)
{
    SndHdr *h = HDR;
    AdlibVoice *va = VOICE;

    s_ticks += 1;                           /* synth time base (91.03 Hz)       */
    if (--h->tempo_ctr == 0) {              /* tempo divider: skip 1 tick in N  */
        h->tempo_ctr = h->tempo_reload;
        return;
    }
    if (s_d1f5) {                           /* new-song key-off                 */
        s_d1f5 = 0;
        silence_all();
    }
    if (s_d1f4) {                           /* stack pop: re-arm the patches    */
        s_d1f4 = 0;
        for (AdlibVoice *v = va; v->stream_ptr != 0xFFFF; ++v) {
            note_on(v, v->keystate);
            set_volume(v, v->volume);
        }
    }
    s_d1f2 = 0;
    for (AdlibVoice *v = va; ; ++v) {
        u16 si = v->stream_ptr;
        if (si == 0xFFFF) break;
        s_d1f2 += 1;
        u8 dur = v->dur_counter;
        v->dur_counter = (u8)(dur - 1);     /* sub [di+7],1 stores the wrap     */
        if (dur > 1) continue;              /* ja: skip while (dur-1) > 0       */
        for (;;) {                          /* fetch loop (commands chain)      */
            u8 al = stream_b(si);
            si += 1;
            if (al & 0x80) {                /* command dispatch @0AE5           */
                u8 cmd = (u8)((al & 0x70) >> 4);
                u8 n   = (u8)(al & 0x0F);
                if (cmd != 7) {
                    switch (cmd) {          /* funcptr table @0xD18C            */
                    case 0: v->dur_shift = n; break;            /* @0B0B */
                    case 1: set_volume(v, n); break;            /* @0B0E */
                    case 2: v->param2 = n; break;               /* @0B66 */
                    case 3: v->alt_shift = n; break;            /* @0B6A */
                    case 4: v->gate = n; break;                 /* @0B6E */
                    case 5: v->opl_channel = n; break;          /* @0B72 */
                    case 6: note_on(v, n); break;               /* @0B76 */
                    }
                } else {
                    switch (n) {            /* ext funcptr table @0xD19A        */
                    case 0:                                     /* GOSUB @0C10  */
                        v->return_ptr = (u16)(si + 2);
                        si = stream_w(si);
                        break;
                    case 1:                                     /* SET_LOOP     */
                        v->loop_count = stream_b(si);
                        si += 1;
                        break;
                    case 2:                                     /* LOOP (DJNZ)  */
                        if (--v->loop_count != 0) si = stream_w(si);
                        else                      si += 2;
                        break;
                    case 3: si = v->return_ptr; break;          /* RETURN       */
                    case 4: si = stream_w(si); break;           /* JUMP         */
                    case 15:                                    /* END @0C30    */
                        si -= 1;            /* un-read the END byte             */
                        s_d1f2 -= 1;        /* drop from the active count       */
                        goto voice_done;    /* store si, next voice             */
                    default: break;         /* table entries 5..14 are 0 (never
                                             * used by the shipped streams)     */
                    }
                }
                continue;                   /* jmp 0x96A: fetch the next byte   */
            }
            /* note event */
            {
                u8 nl = (u8)(al & 0x0F);
                v->prev_note_lo = v->note_lo;
                v->note_lo = nl;
                v->block = (u8)(al >> 4);
                note_update(v, snd_fnum(v->note_lo), v->block);
                u8 d = (v->dur_shift == 7) ? (u8)(0x40 >> v->alt_shift)
                                           : (u8)(0x60 >> v->dur_shift);
                v->dur_counter = d;
            }
            break;
        }
voice_done:
        v->stream_ptr = si;
    }
    if (s_d1f2 == 0 && s_stack != 0) {      /* song/SFX finished: pop the stack */
        s_stack -= 0x10;
        s_d1f4 = 1;
    }
    if (s_d044 != 0) {                      /* percussion one-shot (sfx_play)   */
        u16 bx = s_d044;
        s_d044 = 0;
        h = HDR;
        h->bd |= 0x10;
        opl_write_forced(0xBD, h->bd);
        write5_forced(bx, 0x13);            /* op1 at slot 0x13                 */
        bx += 5;
        write5_forced(bx, 0x10);            /* op2 at slot 0x10                 */
        opl_write_forced(0xC6, patch_b(bx + 5));
        opl_write_forced(0xBD, 0x10);
        opl_write(0xA6, 0x57);
        opl_write(0xB6, 0x00);
        opl_write(0xB6, 0x05);
        opl_write_forced(0xBD, h->bd);
    }
}

/* 5 music ticks = one 18.2 fps frame (phase-locked; see the header comment).
 * Call ONCE per presented frame, AFTER the frame's game logic. */
void snd_frame(void)
{
    for (int i = 0; i < 5; ++i) snd_tick();
    s_frame += 1;
    s_seq = 0;
}

/* extra ticks outside the per-frame cadence — the run_level entry spin
 * (`t24F1 = 3; while (t24F1 == 3);` @0869:0119) burns a measured number of
 * ISR ticks between the race-song trigger and frame 0. */
void snd_run_ticks(int n)
{
    while (n-- > 0) snd_tick();
}

/* ---- sound_init fn143A_0349 (AdLib branch) --------------------------------- */
void snd_init(void)
{
    memset(s_hdr, 0, sizeof s_hdr);
    memset(s_voice, 0, sizeof s_voice);     /* BSS-zero, like the original arrays
                                             * @0xD2E0/D394/D448 (untouched tail
                                             * slots read stream_ptr 0, NOT
                                             * 0xFFFF — load_song writes the
                                             * terminator before any walk) */
    for (int i = 0; i < 3; ++i)
        s_hdr[i].bd = 0x20;                 /* blob init @0xD15C+4: rhythm mode */
    /* hdr0's BD shadow at race start carries BOOT-SONG drum-key residue that
     * is boot-timing dependent EVEN BETWEEN QEMU RUNS (measured 0x39 vs 0x29
     * on two boots): the race song's own percussion traffic overwrites the
     * bits within ~1 s, so only the first few reg-0xBD writes differ. The
     * env knob pins the residue for exact-run register-log comparisons. */
    { const char *e = getenv("FF2_SND_BD");
      if (e) s_hdr[0].bd = (u8)strtol(e, NULL, 0); }
    s_stack = 0; s_d1f2 = 0; s_d1f4 = 0; s_d1f5 = 0; s_d044 = 0;
    s_bd8d = 0; s_b409f = 0; s_b40a0 = 0;
    s_frame = 0; s_seq = 0;

    opl_write(0x01, 0x00);                  /* pre-probe: wave-select enable    */
    /* (the OPL2 presence probe fn143A_04AB = hardware detection, omitted)     */
    opl_write(0x08, 0x00);
    opl_write(0xBD, 0x00);
    opl_write(0xB6, 0x00);
    opl_write(0xA6, 0x00);
    opl_write(0x35, 0x11);
    opl_write(0xA8, 0x57);
    opl_write(0xB8, 0x09);
    opl_write(0xA7, 0x03);
    opl_write(0xB7, 0x0A);
    /* (PIT reload 0x3333 + the INT8 vector install = platform timing, modeled
     * by the run_level/frontend s_isr18 chain + snd_frame's 5 ticks)          */
    snd_load_song(0xD0C2, 0x04, 0xD050);    /* the default/boot song            */
}

/* ---- per-frame state row for --sndtrace / FF2_SNDLOG. COLUMN LAYOUT: the
 * original short set first (BD8E,tempo,d1f2,d044 + 9 x stream,dur,key,note,
 * blk,vol — compatible with the early capsnd captures via min-length compare),
 * then the FULL-record tail: bd,d1f4,d1f5 + 9 x (dur_shift,param2,alt_shift,
 * gate,opl_channel,cur_note,return_ptr,loop_count,prev_note_lo,prev_gate) —
 * every AdlibVoice byte covered (state completeness, user-mandated). */
void snd_state_row(char *out, int cap)
{
    AdlibVoice *va = VOICE;
    int n = snprintf(out, (size_t)cap, "%u,%u,%u,%u",
                     s_stack, HDR->tempo_ctr, s_d1f2, s_d044);
    for (int i = 0; i < 9 && n < cap; ++i)
        n += snprintf(out + n, (size_t)(cap - n), ",%u,%u,%u,%u,%u,%u",
                      va[i].stream_ptr, va[i].dur_counter, va[i].keystate,
                      va[i].note_lo, va[i].block, va[i].volume);
    n += snprintf(out + n, (size_t)(cap - n), ",%u,%u,%u",
                  HDR->bd, s_d1f4, s_d1f5);
    for (int i = 0; i < 9 && n < cap; ++i)
        n += snprintf(out + n, (size_t)(cap - n), ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                      va[i].dur_shift, va[i].param2, va[i].alt_shift, va[i].gate,
                      va[i].opl_channel, va[i].cur_note, va[i].return_ptr,
                      va[i].loop_count, va[i].prev_note_lo, va[i].prev_gate);
}
