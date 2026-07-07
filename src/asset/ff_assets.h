/* ff_assets.h — interface of the FF2 port asset modules (glue between agent code). */
#ifndef FF_ASSETS_H
#define FF_ASSETS_H
#include <stdint.h>
#include "../platform/platform.h"   /* FF_W, FF_H, ff_fb */

/* ---- PowerPacker PP20 depacking (implementation: asset/depack.c) ----
 * Depack a .CPT (PP20) from a file into an allocated buffer.
 * Returns a pointer (malloc'd, freed by the caller) and the length in *out_len, or NULL. */
uint8_t *ff_depack_file(const char *path, int *out_len);

/* ---- IFF ILBM screen (implementation: asset/ilbm.c) ----
 * Load a PP20+ILBM .CPT into the faithful EGA model: out_fb = RAW deinterleaved
 * plane indices (EGA video bytes); out_pal = the per-screen palette the loader
 * programs into the ATC registers (fn0BA8_0BCE remap), as RGB (0..255).
 * Returns 0 on success. */
int ff_load_ilbm_cpt(const char *path, uint8_t out_fb[FF_W*FF_H], uint8_t out_pal[FF_NCOL][3]);
/* VGA-palette variant (front-end / VGA reference): palette = CMAP&0xFC (DAC>>2<<2). */
int ff_load_ilbm_cpt_vga(const char *path, uint8_t out_fb[FF_W*FF_H], uint8_t out_pal[FF_NCOL][3]);

/* fn0BA8_0BCE EGA path: CMAP(16*3 RGB) -> 16 EGA ATC register values. */
void ff_remap_cmap_ega(const uint8_t *cmap, uint8_t atc_out[16]);
/* EGA ATC register value -> display RGB (default 16-colour DAC). */
void ff_ega_atc_rgb(uint8_t atc, uint8_t out[3]);

/* ---- BOB sprite bank (implementation: render/bob.c) ----
 * Planar EGA 4bpp; color 0 = transparent. */
typedef struct BobBank BobBank;
BobBank *ff_bob_load(const char *path);          /* depack + parse BOB.CPT; NULL on error */
BobBank *ff_bob_parse_mem(uint8_t *buf, int len);/* parse an in-memory bank (owns buf) */
void     ff_bob_free(BobBank *b);
int      ff_bob_count(const BobBank *b);
void     ff_bob_dims(const BobBank *b, int id, int *w, int *h);
/* draw sprite id into ff_fb with top-left corner (x,y), color 0 transparent, clipped to screen */
void     ff_bob_blit(const BobBank *b, int id, int x, int y);
/* draw sprite id as a SOLID single-colour silhouette (opaque pixels -> color);
 * faithful port of fn1987_1c73's flash path (Map Mask = t0016-1). See bob.c. */
void     ff_bob_blit_recolor(const BobBank *b, int id, int x, int y, int color);
/* raw planar data for sprite id (4 planes back-to-back, first plane = bit 3) */
const uint8_t *ff_bob_data(const BobBank *b, int id, int *w, int *h);
/* top clip row for sprite blits (fn1987_1c73 playfield window; see bob.c) —
 * set to the playfield origin around display-list draws, 0 otherwise. */
extern int ff_blit_clip_top;
/* fn1187_1C73 shift-6 leading-byte ROM bug (see bob.c): enabled around the
 * 1C73-modeled draw windows, alongside ff_blit_clip_top. */
extern int ff_blit_quirk6;

#endif
