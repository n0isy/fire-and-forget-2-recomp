/* keyboard.c — FAITHFUL port of the INPUT / KEYBOARD module:
 *
 *   INT9 keyboard ISR (194c:007a) + setup fn114C_0012 (194c:0012)
 *                            maintain a 128-byte KEY-STATE array at DGROUP
 *                            0x3FE3 (keystate[scancode & 0x7F] = 0xFF pressed /
 *                            0 released). fn114C_0012 zeroes 0x40 words there.
 *   poll_controls fn0A0D_0002 (120d:0002 / FUN_120d_0002)
 *                            reads that key-state array (+ mouse + joystick) and
 *                            ORs the pressed scancodes into the game's input
 *                            flags (0x24B7 fire, 0x24B9 aux/start, 0x24BB accel,
 *                            0x24BD brake, 0x24BF/0x24C1 steer), then derives the
 *                            missile (wF6B4) / take-off (wF6C2) edge from a
 *                            fire+aux chord.
 *
 * SOURCE OF TRUTH (transliterated, every offset kept): raw/reko/FF2EGA_0A0D.c
 * (fn0A0D_0002, lines 16-106) + raw/reko/FF2EGA_114C.c (fn114C_0012 zeroes the
 * key-state array @ seg15E1:0x3FE3 = DGROUP 0x3FE3).
 *
 * KEY DISCOVERY (proven from the code, not the RUNBOOK summary): the "raw key
 * flags" poll_controls ORs — b401C / b402A / b402C / b402B / b4030 / b402E /
 * b4033 / b402F / b3FFF — are NOT separate variables: they are BYTES OF THE
 * KEY-STATE ARRAY itself, since 0x401C - 0x3FE3 = 0x39 (SPACE), 0x402A = 0x47,
 * 0x402C = 0x49, 0x402B = 0x48 (UP), 0x4030 = 0x4D (RIGHT), 0x402E = 0x4B (LEFT),
 * 0x4033 = 0x50 (DOWN), 0x402F = 0x4C (kp5), 0x3FFF = 0x1C (ENTER). So each
 * "flag" is keystate[scancode]. The full map (keystate off = 0x3FE3 + scancode):
 *   fire  0x24B7 = keystate[0x39 SPACE] | keystate[0x47] | keystate[0x49]
 *   aux   0x24B9 = keystate[0x1C ENTER] | keystate[0x49]
 *   accel 0x24BB = keystate[0x48 UP]
 *   brake 0x24BD = keystate[0x50 DOWN] | keystate[0x4C]
 *   0x24BF       = keystate[0x4D RIGHT]   (camera reads as "steer_left"  -> car right)
 *   0x24C1       = keystate[0x4B LEFT]    (camera reads as "steer_right" -> car left)
 * The mouse (w24C5/C7/C9/CB/CD/CF/D1 + a24C3) and joystick (w24D3.. + w24E3)
 * fields have no hardware source in the port and stay 0, so the ORs below reduce
 * to the keyboard key-state bytes — exactly the original with no stick attached.
 *
 * PORT MODEL: SDL fills GC.input[] each frame; ff_keystate_update() writes those
 * into the key-state array @0x3FE3 (the port's "INT9 ISR"), then poll_controls()
 * runs the fn0A0D_0002 body over it. This gives THREE deterministic control
 * modes through ONE faithful path: (1) attract demo (RLE overrides the flags),
 * (2) past-demo tape, (3) real/scripted keys via GC.input -> keystate -> flags.
 */
#include "ff_game.h"
#include "gmem.h"

/* key-state array base (fn114C_0012 zeroes 0x40 words here). */
#define KEYSTATE 0x3FE3
/* scancodes used by poll_controls (Set 1 make codes). */
#define SC_ENTER 0x1C
#define SC_SPACE 0x39
#define SC_UP    0x48
#define SC_LEFT  0x4B
#define SC_RIGHT 0x4D
#define SC_DOWN  0x50

/* set/clear one key-state byte (the INT9 ISR's per-key store: 0xFF / 0). */
static void ks(int scancode, int down)
{
    GB(KEYSTATE + (scancode & 0x7F)) = down ? 0xFF : 0x00;
}

/* the "INT9 ISR": translate the platform's per-frame key snapshot (GC.input,
 * filled by SDL / the headless replay) into the DGROUP key-state array @0x3FE3,
 * so poll_controls reads it exactly as it reads the real keyboard's. Six keys
 * cover the whole game (fire/start + the 4 arrows); the rest of the 128-byte
 * array stays 0 (unpressed), matching the ISR after fn114C_0012's clear. */
void ff_keystate_update(void)
{
    ks(SC_SPACE, GC.input[FF_K_FIRE]);
    ks(SC_ENTER, GC.input[FF_K_START]);
    ks(SC_UP,    GC.input[FF_K_UP]);
    ks(SC_DOWN,  GC.input[FF_K_DOWN]);
    ks(SC_LEFT,  GC.input[FF_K_LEFT]);
    ks(SC_RIGHT, GC.input[FF_K_RIGHT]);
}

/* poll_controls — faithful port of fn0A0D_0002 (120d:0002). Reads the key-state
 * array (+ the 0-valued mouse/joystick fields) into the input flags, then the
 * fire+aux chord -> missile / take-off edge. The joystick port read (fn114C_01E2)
 * and the pause plane-mask toggle (fn10EF_05B2) are hardware side effects with no
 * game state, omitted; w24E3 (joystick present) is 0 so that whole block is dead. */
void ff_poll_controls(void)
{
    ff_keystate_update();                       /* INT9: GC.input -> keystate */

    /* pause-visual toggle (b3FF7 && b3FFA): keystate[0x14 'T'] && keystate[0x17 'I'];
     * not mapped in the port, so this branch never fires (bF6EB stays 0). The
     * fn10EF_05B2 call is DECODED (ghidra FUN_18ef_05b2 + caller @2246): it is
     * `out 0x3C4, (b37A2==0 ? 0x0B01 : 0x0001)` — VGA SEQUENCER reg 1 (Clocking
     * Mode). 0x0B is the NORMAL value for mode 0Dh (320px = halved dot clock);
     * 0x01 switches to the full dot clock, squeezing the picture into the left
     * half of the CRT scan — the whole "pause" effect. The game keeps running
     * and VRAM is untouched: a pure analog-output distortion with no
     * framebuffer content, outside the port's chunky present model (the b37A2
     * state toggle itself IS ported below). */
    if (GB(0x3FF7) != 0x00 && GB(0x3FFA) != 0x00) {
        if (GB(0xF6EB) == 0x00) GB(0x37A2) ^= 0x01;
        GB(0xF6EB) = 0x01;
    } else {
        GB(0xF6EB) = 0x00;
    }

    /* fn114C_01E2 joystick_read — hardware 0x201 read; skipped (no stick). The
     * mouse/joystick words below (24C3/24C5/24C7/24C9/24CB/24CD/24CF/24D1..) stay
     * 0 in the port, so each OR reduces to its keyboard key-state byte. */
    GW(0x24C1) = (u16)GB(0x402E) | GW(0x24CD) | GW(0x24D9);              /* LEFT arrow  */
    GW(0x24BF) = (u16)GB(0x4030) | GW(0x24CB) | GW(0x24D7);              /* RIGHT arrow */
    GW(0x24BB) = (u16)GB(0x402B) | GW(0x24C7) | GW(0x24D3);             /* UP  (accel) */
    GW(0x24BD) = (u16)GB(0x4033) | GW(0x24C9) | GW(0x24D5) | (u16)GB(0x402F); /* DOWN (brake) */
    GW(0x24B7) = (u16)GB(0x401C) | GW(0x24C3) | GW(0x24CF)
               | (u16)GB(0x402A) | (u16)GB(0x402C);                     /* SPACE|kp7|kp9 fire */
    GW(0x24B9) = (u16)GB(0x3FFF) | GW(0x24C5) | GW(0x24D1) | (u16)GB(0x402C); /* ENTER|kp9 aux */

    /* the w24E3 (joystick present) block of fn0A0D_0002 is skipped: w24E3 == 0. */

    /* missile / take-off from the fire+aux chord (fn0A0D_0002 @87-104). */
    GW(0xF6B4) = 0x00;                           /* wF6B4 missile */
    GW(0xF6C2) = 0x00;                           /* wF6C2 take-off */
    if (GW(0x24B9) != 0x00) {                    /* aux/start held */
        if (GB(0xF6EA) == 0x00) {                /* rising edge of aux (tF6EA) */
            if (GW(0x24B7) != 0x00) GW(0xF6B4) = 0x01;   /* fire+aux -> MISSILE */
            else { GW(0x24E9) = 0x00; GW(0xF6C2) = 0x01; }  /* aux alone -> take-off */
        }
        GW(0x24B7) = 0x00;                        /* aux consumes the fire flag */
    }
    GB(0xF6EA) = (u8)GW(0x24B9);                  /* tF6EA = aux edge latch */
}
