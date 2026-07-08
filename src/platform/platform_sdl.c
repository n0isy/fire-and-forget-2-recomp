/* platform_sdl.c — SDL2 implementation of platform.h. */
#include "platform.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

uint8_t ff_fb[FF_W * FF_H];

/* standard 16-color EGA palette (0..255 per channel) */
const uint8_t ff_ega_palette[FF_NCOL][3] = {
    {  0,  0,  0},{  0,  0,170},{  0,170,  0},{  0,170,170},
    {170,  0,  0},{170,  0,170},{170, 85,  0},{170,170,170},
    { 85, 85, 85},{ 85, 85,255},{ 85,255, 85},{ 85,255,255},
    {255, 85, 85},{255, 85,255},{255,255, 85},{255,255,255},
};

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t      pal[FF_NCOL];           /* ARGB8888 LUT */
static uint8_t       keys[FF_K_COUNT];
static uint32_t      next_tick;

void ff_set_palette(const uint8_t rgb[FF_NCOL][3]) {
    for (int i = 0; i < FF_NCOL; i++)
        pal[i] = 0xFF000000u | (rgb[i][0] << 16) | (rgb[i][1] << 8) | rgb[i][2];
}

int ff_init(const char *title, int scale) {
    if (scale < 1) scale = 3;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           FF_W * scale, FF_H * scale, SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); return -1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(ren, FF_W, FF_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, FF_W, FF_H);
    ff_set_palette(ff_ega_palette);
    next_tick = SDL_GetTicks();
    return 0;
}

void ff_shutdown(void) {
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}

void ff_clear(uint8_t c) { memset(ff_fb, c, sizeof ff_fb); }

void ff_present(void) {
    uint32_t *px; int pitch;
    SDL_LockTexture(tex, NULL, (void **)&px, &pitch);
    for (int i = 0; i < FF_W * FF_H; i++) px[i] = pal[ff_fb[i] & 15];
    SDL_UnlockTexture(tex);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

static int scancode_to_key(SDL_Scancode s) {
    switch (s) {
    case SDL_SCANCODE_UP:    return FF_K_UP;
    case SDL_SCANCODE_DOWN:  return FF_K_DOWN;
    case SDL_SCANCODE_LEFT:  return FF_K_LEFT;
    case SDL_SCANCODE_RIGHT: return FF_K_RIGHT;
    case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_LCTRL: return FF_K_FIRE;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_LSHIFT: return FF_K_START;   /* aux/start button (t24B9) */
    case SDL_SCANCODE_F2:    return FF_K_MUTE;     /* original F2: mute music  */
    case SDL_SCANCODE_F3:    return FF_K_SFX;      /* original F3: one-shots   */
    case SDL_SCANCODE_F5:    return FF_K_EXIT;     /* original F5: abort race  */
    case SDL_SCANCODE_ESCAPE:return FF_K_ESC;
    default: return -1;
    }
}

int ff_poll(void) {
    /* MIN-ONE-FRAME LATCH: the game samples keys[] once per 18.2fps frame
     * (~55 ms), and this drains the whole SDL queue in one go — a quick tap
     * whose KEYDOWN and KEYUP both arrive in the same batch used to cancel
     * out before the game ever saw it (menu START taps were being lost). A
     * release that lands in the same poll as its press is deferred to the
     * NEXT poll, so every physical press is visible for at least one frame. */
    static uint8_t deferred_up[FF_K_COUNT];
    uint8_t downed_now[FF_K_COUNT] = {0};
    for (int k = 0; k < FF_K_COUNT; k++)
        if (deferred_up[k]) { keys[k] = 0; deferred_up[k] = 0; }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return 0;
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            int k = scancode_to_key(e.key.keysym.scancode);
            if (k < 0) continue;
            if (e.type == SDL_KEYDOWN) {
                keys[k] = 1;
                downed_now[k] = 1;
            } else if (downed_now[k]) {
                deferred_up[k] = 1;     /* same-batch tap: release next poll */
            } else {
                keys[k] = 0;
            }
        }
    }
    return keys[FF_K_ESC] ? 0 : 1;
}

int  ff_key(int k) { return (k >= 0 && k < FF_K_COUNT) ? keys[k] : 0; }

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
/* On-screen touch controls (web): set a key's state directly from JS. ff_poll()
 * only writes keys[] on real keyboard events, so a value set here persists until
 * JS clears it — the mobile virtual joystick/buttons drive the game through the
 * SAME keys[] array the real keyboard does. (JS does the ref-counting so shared
 * keys, e.g. FIRE used by both the fire and missile buttons, behave.) Called as
 * Module._ff_web_key(keyIndex, down); KEEPALIVE auto-exports it. */
EMSCRIPTEN_KEEPALIVE void ff_web_key(int k, int down) {
    if (k >= 0 && k < FF_K_COUNT) keys[k] = (uint8_t)(down ? 1 : 0);
}
#endif

void ff_wait_frame(void) {
    /* Faithful frame rate: the original syncs each frame to one bBCBB tick of
     * the timer ISR (fn1187_0136), and bBCBB advances once per 4 PIT ticks with
     * the PIT reloaded to 0x4000 -> 1193182/16384/4 = 18.207 Hz (the AdLib
     * branch 1193182/13107/5 gives the same 18.21). Period = 54925 us. */
    static uint32_t acc_us;
    acc_us += 54925;
    next_tick += acc_us / 1000;
    acc_us %= 1000;
    uint32_t now = SDL_GetTicks();
    if (next_tick > now) SDL_Delay(next_tick - now);
    else next_tick = now;
}

void ff_screenshot_ppm(const char *path, const uint8_t rgb[FF_NCOL][3]) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", FF_W, FF_H);
    for (int i = 0; i < FF_W * FF_H; i++) {
        uint8_t c = ff_fb[i] & 15;
        fputc(rgb[c][0], f); fputc(rgb[c][1], f); fputc(rgb[c][2], f);
    }
    fclose(f);
}
