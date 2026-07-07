/* platform.h — platform abstraction for the FF2 port (DOS/EGA -> Linux/SDL2).
 * Game code uses ONLY this interface; it knows nothing about SDL/hardware. */
#ifndef FF_PLATFORM_H
#define FF_PLATFORM_H
#include <stdint.h>

#define FF_W 320
#define FF_H 200
#define FF_NCOL 16

/* framebuffer: palette indices 0..15 (like EGA mode 0Dh, but linear) */
extern uint8_t ff_fb[FF_W * FF_H];

/* keys (minimal set for arcade controls).
 * FF_K_START = the aux/"start" button (t24B9): inserts-and-starts in the menu,
 * take-off/missile chord in-race (poll_controls fn0A0D_0002). */
enum { FF_K_UP, FF_K_DOWN, FF_K_LEFT, FF_K_RIGHT, FF_K_FIRE, FF_K_START, FF_K_ESC, FF_K_COUNT };

/* window lifecycle (needed only by the native playable mode) */
int  ff_init(const char *title, int scale);
void ff_shutdown(void);

/* video */
void ff_set_palette(const uint8_t rgb[FF_NCOL][3]); /* components 0..255 */
void ff_present(void);          /* ff_fb -> screen */
void ff_clear(uint8_t color);

/* input / timing */
int  ff_poll(void);             /* process events; 0 = exit requested */
int  ff_key(int k);             /* is key FF_K_* pressed (1/0) */
void ff_wait_frame(void);       /* frame sync ~70 Hz */

/* utility: dump the frame to PPM (for headless check, no window) */
void ff_screenshot_ppm(const char *path, const uint8_t rgb[FF_NCOL][3]);

/* standard EGA palette (16 colors) */
extern const uint8_t ff_ega_palette[FF_NCOL][3];

#endif
