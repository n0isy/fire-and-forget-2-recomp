/* depack.c -- PowerPacker PP20 decruncher for Fire & Forget II .CPT assets.
 *
 * Mirrors the original engine path depack_powerpacker (Reko 143A:00B2 /
 * Ghidra 1c3a:00b2 / linear 0x1c452), implemented from the working reference
 * ports tools/pp20_unpack.py and intermediate/F4/depack_powerpacker__143A_00B2.c.
 *
 * The PP20 bitstream is consumed from the END of the buffer BACKWARD and the
 * output is produced BACKWARD as well.  Tokens are either literal runs or
 * back-reference copies (LZ).  This is the libxmp/amigadepack algorithm.
 *
 * Only stdio/stdlib/string are used (no external libraries).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ff_assets.h"

/* Backward MSB-first bit reader over the compressed stream. */
typedef struct {
    const uint8_t *src;   /* base of compressed stream                       */
    long           pos;   /* next byte is read at src[--pos] (reads backward) */
    uint64_t       buf;   /* accumulated bits (filled at the high end)        */
    int            left;  /* number of valid bits currently in buf           */
    int            err;   /* set when the stream underflows                  */
} PPBits;

static unsigned pp_read_bits(PPBits *b, int n)
{
    unsigned val = 0;
    while (b->left < n) {
        if (b->pos <= 0) { b->err = 1; return 0; }
        b->pos--;
        b->buf |= (uint64_t)b->src[b->pos] << b->left;
        b->left += 8;
    }
    b->left -= n;
    while (n--) {
        val = (val << 1) | (unsigned)(b->buf & 1u);
        b->buf >>= 1;
    }
    return val;
}

/* Decrunch a PP20 image. Returns malloc'd output (caller frees) + length. */
static uint8_t *pp20_decrunch(const uint8_t *data, long len, int *out_len)
{
    if (len < 12 || memcmp(data, "PP20", 4) != 0)
        return NULL;

    const uint8_t *offlen = data + 4;                    /* 4 "efficiency" bytes */
    long dest_len = ((long)data[len - 4] << 16) |
                    ((long)data[len - 3] << 8)  |
                     (long)data[len - 2];
    int  skip = data[len - 1];

    if (dest_len <= 0) return NULL;
    uint8_t *out = malloc((size_t)dest_len);
    if (!out) return NULL;

    PPBits b;
    b.src  = data + 8;          /* compressed stream is data[8 .. len-4) */
    b.pos  = (len - 4) - 8;     /* read backward starting just past it   */
    b.buf  = 0;
    b.left = 0;
    b.err  = 0;

    long o = dest_len;          /* output is written backward: out[--o] */
    pp_read_bits(&b, skip);     /* discard alignment bits at the start  */

    long written = 0;
    while (written < dest_len && !b.err) {
        if (pp_read_bits(&b, 1) == 0) {                  /* literal run */
            long todo = 1; unsigned g;
            do { g = pp_read_bits(&b, 2); todo += g; } while (g == 3 && !b.err);
            while (todo-- > 0 && o > 0) {
                out[--o] = (uint8_t)pp_read_bits(&b, 8);
                written++;
            }
            if (written >= dest_len) break;
        }
        /* back-reference copy */
        unsigned x = pp_read_bits(&b, 2);
        int  offbits = offlen[x];
        long todo    = (long)x + 2;
        long offset;
        if (x == 3) {
            if (pp_read_bits(&b, 1) == 0) offbits = 7;
            offset = pp_read_bits(&b, offbits);
            unsigned g;
            do { g = pp_read_bits(&b, 3); todo += g; } while (g == 7 && !b.err);
        } else {
            offset = pp_read_bits(&b, offbits);
        }
        while (todo-- > 0 && o > 0) {
            if (o + offset >= dest_len) { b.err = 1; break; }
            out[o - 1] = out[o + offset];
            o--;
            written++;
        }
    }

    if (b.err && written < dest_len) {
        free(out);
        return NULL;
    }
    if (out_len) *out_len = (int)dest_len;
    return out;
}

/* Read a .CPT file; if it begins with the PP20 magic, decrunch it, otherwise
 * return the raw bytes unchanged. Returns malloc'd buffer (caller frees). */
uint8_t *ff_depack_file(const char *path, int *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    rewind(f);

    uint8_t *raw = malloc((size_t)len);
    if (!raw) { fclose(f); return NULL; }
    if (fread(raw, 1, (size_t)len, f) != (size_t)len) {
        free(raw); fclose(f); return NULL;
    }
    fclose(f);

    if (len >= 4 && memcmp(raw, "PP20", 4) == 0) {
        uint8_t *out = pp20_decrunch(raw, len, out_len);
        free(raw);
        return out;                 /* NULL on decrunch failure */
    }

    if (out_len) *out_len = (int)len;
    return raw;                      /* not packed: hand back as-is */
}
