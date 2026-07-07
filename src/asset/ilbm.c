/* ilbm.c -- IFF ILBM screen loader for Fire & Forget II (.CPT assets).
 *
 * Pipeline (mirrors the original engine, see intermediate/assets/ilbm.c):
 *     <NAME>.CPT  --ff_depack_file (PP20)-->  IFF ILBM image in RAM
 *                 --parse FORM/ILBM { BMHD, CMAP, BODY }
 *                 --ByteRun1 (PackBits) decode of BODY
 *                 --deinterleave 4 bitplanes -> 320x200 8-bit index buffer
 *                 --CMAP (16*3 bytes) -> EGA palette via nearest-colour remap
 *
 * PALETTE (faithful port of fn0BA8_0BCE @ 0BA8:0BCE, the EGA path):
 *   The .CPT/ILBM CMAP holds the Amiga palette.  The EGA executable IGNORES it
 *   as RGB; instead, for each of the 16 CMAP colours it finds the nearest colour
 *   (L1 / Manhattan distance) in a fixed 16-entry reference palette baked into
 *   DGROUP at 0x3874, then emits the matching EGA Attribute-Controller register
 *   value from a table at DGROUP 0x3864.  load_level (fn0BA8_0AC5) writes the
 *   raw BODY indices to video memory unchanged and loads those 16 ATC values
 *   into the EGA palette registers (fn1187_2A6F).  The on-screen colours are
 *   therefore the standard 16-colour EGA default palette.
 *
 *   This port collapses that to a single index remap (path-b equivalent): each
 *   pixel's raw index is replaced by the default-EGA palette index that the
 *   original's ATC register would have displayed, so callers can render through
 *   the fixed default EGA palette.  Verified bit-exact vs DOSBox machine=ega.
 *
 * No external libraries: only stdio/stdlib/string.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ff_assets.h"

/* IFF is big-endian. */
static uint32_t be32(const uint8_t *p){ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static uint16_t be16(const uint8_t *p){ return (uint16_t)(p[0]<<8 | p[1]); }

/* IFF ByteRun1 == Apple/Amiga PackBits. Decodes up to `outlen` bytes.
 *   ctrl >= 0 : copy (ctrl+1) literal bytes
 *   ctrl <  0 : emit (1-ctrl) copies of the next byte   (-128 is a no-op)
 */
static void unpack_byterun1(const uint8_t *src, long srclen,
                            uint8_t *dst, long outlen)
{
    long si = 0, di = 0;
    while (di < outlen && si < srclen) {
        signed char ctrl = (signed char)src[si++];
        if (ctrl >= 0) {
            int n = ctrl + 1;
            while (n-- && di < outlen && si < srclen) dst[di++] = src[si++];
        } else if (ctrl != -128) {
            int n = 1 - ctrl;
            if (si >= srclen) break;
            uint8_t v = src[si++];
            while (n-- && di < outlen) dst[di++] = v;
        }
    }
}

/* fn0BA8_0BCE EGA-path data (static constants from FF2EGA.EXE DGROUP):
 *   ega_ref[16]  reference RGB palette @ DGROUP 0x3874 (used for L1 matching).
 *                Note entry 6 is dark-yellow (170,170,0) — the EGA "brown fix"
 *                lives in the ATC register value, not here.
 *   ega_atc[16]  output table @ DGROUP 0x3864: reference-index -> ATC register
 *                value. Entry 10 deliberately folds light-green to green (0x02). */
static const uint8_t ega_ref[16][3] = {
    {  0,  0,  0},{  0,  0,170},{  0,170,  0},{  0,170,170},
    {170,  0,  0},{170,  0,170},{170,170,  0},{170,170,170},
    { 85, 85, 85},{ 85, 85,255},{ 85,255, 85},{ 85,255,255},
    {255, 85, 85},{255, 85,255},{255,255, 85},{255,255,255},
};
static const uint8_t ega_atc[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x10,0x11,0x02,0x13,0x14,0x15,0x16,0x17,
};

/* Default 16-colour EGA palette RGB (same order as platform ff_ega_palette);
 * the ATC register value maps to one of these via idx = (v&7)|((v>>1)&8). */
static void ega_default_rgb(int idx, uint8_t out[3])
{
    static const uint8_t P[16][3] = {
        {  0,  0,  0},{  0,  0,170},{  0,170,  0},{  0,170,170},
        {170,  0,  0},{170,  0,170},{170, 85,  0},{170,170,170},
        { 85, 85, 85},{ 85, 85,255},{ 85,255, 85},{ 85,255,255},
        {255, 85, 85},{255, 85,255},{255,255, 85},{255,255,255},
    };
    out[0]=P[idx&15][0]; out[1]=P[idx&15][1]; out[2]=P[idx&15][2];
}

/* Faithful port of fn0BA8_0BCE (EGA path): for each of the 16 file CMAP colours,
 * emit the EGA Attribute-Controller register value the loader programs (the
 * original then loads these 16 values into the ATC palette via fn1187_2A6F).
 * cmap holds 16*3 bytes (8-bit RGB). */
void ff_remap_cmap_ega(const uint8_t *cmap, uint8_t atc_out[16])
{
    for (int c = 0; c < 16; ++c) {              /* for wLoc04 = 0..0x0F */
        int best = 0, bestd = 0x40;             /* wLoc08 init 0x40, /3 threshold */
        const uint8_t *src = cmap + c * 3;
        for (int r = 0; r < 16; ++r) {          /* scan 16 reference colours */
            int d = abs((int)src[0] - ega_ref[r][0])
                  + abs((int)src[1] - ega_ref[r][1])
                  + abs((int)src[2] - ega_ref[r][2]);
            d /= 3;                              /* dx_ax /16 0x03 */
            if (d < bestd) { bestd = d; best = r; }
        }
        atc_out[c] = ega_atc[best];             /* ds->*(best + 0x3864) */
    }
}

/* EGA ATC register value -> display RGB (default 16-colour DAC). The ATC value's
 * low nibble is the 0..7 colour, bit4 adds the 8-step (and 0x06 is the brown
 * fix, handled by ega_default_rgb's entry 6). */
void ff_ega_atc_rgb(uint8_t atc, uint8_t out[3])
{
    ega_default_rgb((atc & 7) | ((atc >> 1) & 8), out);
}

/* Load PP20+ILBM .CPT into a 320x200 index buffer and a 16x3 palette.
 * Returns 0 on success, non-zero on failure. */
/* Depack + parse an ILBM .CPT into out_fb (raw deinterleaved plane indices) and
 * cmap (16*3 bytes, the IFF CMAP). Returns 0 on success. The two public loaders
 * below wrap this and apply either the EGA-ATC or the VGA palette. */
static int load_ilbm_core(const char *path, uint8_t out_fb[FF_W*FF_H], uint8_t cmap[FF_NCOL*3])
{
    int len = 0;
    uint8_t *buf = ff_depack_file(path, &len);
    if (!buf) return 1;

    if (len < 12 || memcmp(buf, "FORM", 4) || memcmp(buf + 8, "ILBM", 4)) {
        free(buf);
        return 1;                       /* not an IFF ILBM (e.g. BOB, DECx, NUC) */
    }

    int w = 0, h = 0, nplanes = 0, comp = 0;
    const uint8_t *body = NULL; long bodylen = 0;
    memset(cmap, 0, (size_t)FF_NCOL * 3);

    long pos = 12;                       /* first chunk after FORM hdr + "ILBM" */
    while (pos + 8 <= len) {
        const uint8_t *id  = buf + pos;
        uint32_t       csz = be32(buf + pos + 4);
        const uint8_t *cd  = buf + pos + 8;
        if (pos + 8 + (long)csz > len) csz = (uint32_t)(len - pos - 8);

        if (!memcmp(id, "BMHD", 4)) {
            w       = be16(cd + 0);
            h       = be16(cd + 2);
            nplanes = cd[8];
            comp    = cd[10];
        } else if (!memcmp(id, "CMAP", 4)) {
            int ncol = (int)csz / 3;
            if (ncol > FF_NCOL) ncol = FF_NCOL;
            memcpy(cmap, cd, (size_t)ncol * 3);
        } else if (!memcmp(id, "BODY", 4)) {
            body = cd; bodylen = csz;
        }
        pos += 8 + csz + (csz & 1);      /* chunks are word-aligned */
    }

    if (!body || w <= 0 || h <= 0 || nplanes <= 0) { free(buf); return 1; }

    int  rowbytes  = ((w + 15) / 16) * 2;        /* 16-bit aligned -> 40 for 320 */
    long planarlen = (long)h * nplanes * rowbytes;
    uint8_t *planar = calloc(1, (size_t)planarlen);
    if (!planar) { free(buf); return 1; }

    if (comp == 1)
        unpack_byterun1(body, bodylen, planar, planarlen);
    else
        memcpy(planar, body, bodylen < planarlen ? (size_t)bodylen : (size_t)planarlen);

    /* deinterleave row-interleaved bitplanes into the chunky index buffer */
    memset(out_fb, 0, (size_t)FF_W * FF_H);
    int cw = w < FF_W ? w : FF_W;
    int ch = h < FF_H ? h : FF_H;
    for (int y = 0; y < ch; ++y) {
        const uint8_t *prow = planar + (long)y * nplanes * rowbytes;
        for (int p = 0; p < nplanes; ++p) {
            const uint8_t *plane = prow + (long)p * rowbytes;
            for (int x = 0; x < cw; ++x) {
                int bit = (plane[x >> 3] >> (7 - (x & 7))) & 1;
                out_fb[(long)y * FF_W + x] |= (uint8_t)(bit << p);
            }
        }
    }
    free(planar);

    /* Faithful EGA model: out_fb keeps the RAW deinterleaved plane indices (the
     * bytes the original writes to video memory via fn10EF_0521). The per-screen
     * palette is the ATC programming the loader performs: fn0BA8_0BCE maps each
     * CMAP colour to an ATC register value (fn1187_2A6F), which the EGA DAC shows
     * as one of the default 16 colours. So out_pal[i] = RGB of ATC[i]. Anything
     * drawn afterwards (font/sprites) writes raw indices into the same buffer and
     * is coloured by this same palette — exactly like the hardware. */
    free(buf);
    return 0;
}

/* EGA path (default 16-colour DAC via the fn0BA8_0BCE ATC remap) — the faithful
 * palette on an EGA machine; verified bit-exact vs DOSBox machine=ega. */
int ff_load_ilbm_cpt(const char *path, uint8_t out_fb[FF_W*FF_H], uint8_t out_pal[FF_NCOL][3])
{
    uint8_t cmap[FF_NCOL * 3];
    if (load_ilbm_core(path, out_fb, cmap)) return 1;
    uint8_t atc[16];
    ff_remap_cmap_ega(cmap, atc);
    for (int i = 0; i < FF_NCOL; ++i)
        ff_ega_atc_rgb(atc[i], out_pal[i]);
    return 0;
}

/* VGA path (the front-end / gameplay reference on a VGA machine, w24F9==4): the
 * loader programs the VGA DAC straight from the IFF CMAP (remap_palette_ega VGA
 * branch: dest[i] = CMAP[i] >> 2), so the displayed RGB = (CMAP>>2)<<2 = CMAP&0xFC.
 * This is the chrome/metallic title palette the QEMU std-VGA reference shows. */
int ff_load_ilbm_cpt_vga(const char *path, uint8_t out_fb[FF_W*FF_H], uint8_t out_pal[FF_NCOL][3])
{
    uint8_t cmap[FF_NCOL * 3];
    if (load_ilbm_core(path, out_fb, cmap)) return 1;
    for (int i = 0; i < FF_NCOL; ++i) {
        out_pal[i][0] = (uint8_t)(cmap[i*3+0] & 0xFC);
        out_pal[i][1] = (uint8_t)(cmap[i*3+1] & 0xFC);
        out_pal[i][2] = (uint8_t)(cmap[i*3+2] & 0xFC);
    }
    return 0;
}
