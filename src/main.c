/* main.c — FF2 port skeleton. So far: EGA palette + frame loop.
 * Modes:
 *   ./ff2            — SDL2 window, test pattern, exit on ESC/close.
 *   ./ff2 --shot p   — headless: draw the pattern into the frame and save PPM (no window).
 */
#include "platform/platform.h"
#include <string.h>
#include <stdio.h>

/* test frame: 16 vertical bars (one per palette color) + border */
static void draw_test_pattern(void) {
    for (int y = 0; y < FF_H; y++)
        for (int x = 0; x < FF_W; x++)
            ff_fb[y * FF_W + x] = (x * FF_NCOL) / FF_W;
    for (int x = 0; x < FF_W; x++) { ff_fb[x] = 15; ff_fb[(FF_H-1)*FF_W + x] = 15; }
    for (int y = 0; y < FF_H; y++) { ff_fb[y*FF_W] = 15; ff_fb[y*FF_W + FF_W-1] = 15; }
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "--shot") == 0) {     /* headless */
        memset(ff_fb, 0, sizeof ff_fb);
        draw_test_pattern();
        ff_screenshot_ppm(argv[2], ff_ega_palette);
        printf("wrote %s (%dx%d, EGA-16)\n", argv[2], FF_W, FF_H);
        return 0;
    }

    if (ff_init("Fire & Forget II (recomp)", 3) != 0) return 1;
    while (ff_poll()) {
        draw_test_pattern();
        ff_present();
        ff_wait_frame();
    }
    ff_shutdown();
    return 0;
}
