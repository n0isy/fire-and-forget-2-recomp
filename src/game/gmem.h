/* gmem.h — raw DGROUP access for FAITHFUL transliteration of decompiled code.
 *
 * The whole 16-bit DGROUP is loaded as a blob into `G` (struct Globals, byte-exact
 * layout). In the decompiled sources every data access is DGROUP-relative, so a
 * line-by-line port resolves mechanically:
 *
 *   Reko/Ghidra                         ->  faithful port
 *   ds->wNNNN  / DAT_xxxx_NNNN (word)   ->  GW(0xNNNN)
 *   ds->bNNNN                  (byte)   ->  GB(0xNNNN)
 *   ds->tNNNN  (signed word use)        ->  GI(0xNNNN)
 *   ds->aNNNN  / ds->*(x + 0xNNNN)      ->  GPTR(0xNNNN)   (then index/cast)
 *   (ds->*((word16)k + 0xE4CC))         ->  GW(0xE4CC + (k))   etc.
 *   SEQ(seg, off)                       ->  GPTR(off)      (seg is always DGROUP)
 *   SLICE(p, word16, 16)                ->  (unused: segment half — DGROUP)
 *   far CALL / function pointer         ->  call the named ported C function
 *
 * Keep EVERY constant, threshold, table index and field offset exactly as in the
 * decompiled source. Do NOT approximate, centralize, or invent values.
 */
#ifndef FF_GMEM_H
#define FF_GMEM_H
#include "../ff2.h"   /* struct Globals G, u8/u16/i16, accessors, DG_* */

#define GB(o)   (*(u8  *)((u8 *)&G + (o)))   /* byte   at DGROUP offset o */
#define GW(o)   (*(u16 *)((u8 *)&G + (o)))   /* word   at o (unsigned)    */
#define GI(o)   (*(i16 *)((u8 *)&G + (o)))   /* word   at o (signed)      */
#define GD(o)   (*(u32 *)((u8 *)&G + (o)))   /* dword  at o               */
#define GPTR(o) ((u8 *)&G + (o))             /* pointer into DGROUP at o  */

#endif
