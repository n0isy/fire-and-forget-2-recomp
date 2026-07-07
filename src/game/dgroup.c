/* dgroup.c - the single definition of the global game state `G`, plus the
 * loader that fills it with the original initialized DGROUP image.
 *
 * The 16-bit large-model engine kept all of its globals in one DGROUP data
 * segment (0x10000 bytes). `struct Globals` (gen/ff2_globals.h) reproduces that
 * layout byte-for-byte, so loading the extracted blob (assets/dgroup.bin, see
 * tools/extract_dgroup.py) directly into &G makes every static table - sine,
 * enemy names/prototypes, F-number table, VM bytecode, song data - available at
 * runtime exactly as the original binary saw it.
 *
 * This translation unit is the ONE place that defines `G`; everything else
 * references it via `extern struct Globals G;` from ff2_globals.h.
 */
#include <stdio.h>
#include <string.h>

#include "ff2.h"   /* struct Globals, accessors, platform primitives */

/* THE definition of the global game state. Nothing else defines G. */
struct Globals G;

/* Load the initialized DGROUP blob from `path` into G.
 * Zeroes G first, then copies min(file_len, sizeof G) bytes (the on-disk image
 * is the initialized prefix of DGROUP; the BSS tail stays zero).
 * Returns 0 on success, -1 on failure (file not found / read error). */
int ff_load_dgroup(const char *path)
{
    FILE *f;
    long file_len;
    size_t to_read, got;

    memset(&G, 0, sizeof G);

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ff_load_dgroup: cannot open '%s'\n", path);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    file_len = ftell(f);
    if (file_len < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);

    to_read = (size_t)file_len;
    if (to_read > sizeof G)
        to_read = sizeof G;

    got = fread(&G, 1, to_read, f);
    fclose(f);

    if (got != to_read) {
        fprintf(stderr, "ff_load_dgroup: short read on '%s' (%zu/%zu)\n",
                path, got, to_read);
        return -1;
    }

    return 0;
}
