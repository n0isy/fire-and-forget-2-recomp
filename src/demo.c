/* demo.c — vertical-slice demo for the FF2 port.
 * Proves the live asset pipeline: PowerPacker depack -> IFF ILBM screens +
 * BOB planar sprite bank, rendered through the SDL2 platform (and WASM).
 *
 * Modes:
 *   ff2demo                  SDL2 window; LEFT/RIGHT switch scene, auto-advance, ESC quits.
 *   ff2demo --shot N out.ppm headless: render scene N to a PPM (no window).
 * Asset root: $FF2_ASSETS or "assets" (expects <root>/cpt/<NAME>.CPT).
 */
#include "platform/platform.h"
#include "asset/ff_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static char       g_assets[256] = "assets";
static BobBank   *g_bob;
static uint8_t    g_pal[FF_NCOL][3];

/* full-screen ILBM background; also sets g_pal */
static int load_bg(const char *cpt) {
    char p[320];
    snprintf(p, sizeof p, "%s/cpt/%s", g_assets, cpt);
    return ff_load_ilbm_cpt(p, ff_fb, g_pal);
}

/* sprite roster over a level palette: grid of BOB sprites on a dark background */
static void scene_roster(const char *palette_cpt) {
    static const int ids[] = { 0,1,2,3, 54,55, 60,80,100,130,160,190, 232,234,235 };
    if (load_bg(palette_cpt) != 0) { ff_clear(0); }
    /* dim the loaded background to a flat dark fill so sprites read clearly */
    ff_clear(8);                                   /* EGA dark gray */
    int x = 6, y = 12, rowh = 0;
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        int w = 0, h = 0;
        ff_bob_dims(g_bob, ids[i], &w, &h);
        if (x + w > FF_W - 4) { x = 6; y += rowh + 6; rowh = 0; }
        ff_bob_blit(g_bob, ids[i], x, y);
        x += w + 8;
        if (h > rowh) rowh = h;
    }
}

/* scene table: index -> renderer */
static void render_scene(int n) {
    switch (n) {
    case 0: if (load_bg("TITRE.CPT"))    ff_clear(0); break;
    case 1: if (load_bg("PRES.CPT"))     ff_clear(0); break;
    case 2: if (load_bg("TITUS.CPT"))    ff_clear(0); break;
    case 3: scene_roster("ETAPE.CPT");                break;   /* live BOB sprites */
    case 4: if (load_bg("CONG.CPT"))     ff_clear(0); break;
    case 5: if (load_bg("GAMEOVER.CPT")) ff_clear(0); break;
    default: ff_clear(0); break;
    }
}
#define N_SCENES 6

/* per-frame state (shared by native loop and the emscripten callback) */
static int g_scene = 0, g_t = 0, g_prev_l = 0, g_prev_r = 0, g_quit = 0;

static void frame(void) {
    if (!ff_poll()) {
        g_quit = 1;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }
    int l = ff_key(FF_K_LEFT), r = ff_key(FF_K_RIGHT), change = 0;
    if (l && !g_prev_l) { g_scene = (g_scene + N_SCENES - 1) % N_SCENES; change = 1; }
    if (r && !g_prev_r) { g_scene = (g_scene + 1) % N_SCENES; change = 1; }
    g_prev_l = l; g_prev_r = r;
    if (++g_t >= 180) { g_scene = (g_scene + 1) % N_SCENES; change = 1; }  /* ~2.5s auto */
    if (change) { g_t = 0; render_scene(g_scene); ff_set_palette((const uint8_t(*)[3])g_pal); }
    ff_present();
}

int main(int argc, char **argv) {
    const char *env = getenv("FF2_ASSETS");
    if (env) snprintf(g_assets, sizeof g_assets, "%s", env);

    /* load the sprite bank once */
    char p[320];
    snprintf(p, sizeof p, "%s/cpt/BOB.CPT", g_assets);
    g_bob = ff_bob_load(p);
    if (!g_bob) { fprintf(stderr, "failed to load BOB bank from %s\n", p); }

    /* headless screenshot mode */
    if (argc >= 4 && strcmp(argv[1], "--shot") == 0) {
        render_scene(atoi(argv[2]));
        ff_screenshot_ppm(argv[3], g_pal[0][0] || g_pal[15][2] ? (const uint8_t(*)[3])g_pal
                                                               : ff_ega_palette);
        printf("scene %s -> %s\n", argv[2], argv[3]);
        return 0;
    }

    if (ff_init("Fire & Forget II — recomp (asset slice)", 3) != 0) return 1;
    render_scene(0);
    ff_set_palette((const uint8_t(*)[3])g_pal);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);   /* browser-driven loop */
#else
    while (!g_quit) { frame(); ff_wait_frame(); }
    ff_shutdown();
    ff_bob_free(g_bob);
#endif
    return 0;
}
