#!/usr/bin/env python3
"""wavedis.py -- LOSSLESS disassembler of the Fire & Forget II wave/path-VM
bytecode (DGROUP 0xA0..0xCB2, script directory @0x94) into the original DSL.

The mnemonics are the ORIGINAL tool's keywords, recovered from the keyword
table the game still carries in its data segment @0x355A:
    CLEAR GETCHAR PUT WHILE GOTO SETDIST GOSUB RETURN BEQ WAVE
    DEFB END INCLUDE ERREUR        (assembler directives; DEFB used here)

Guarantees: every byte of the region is emitted exactly once (op / record
block / DEFB island), in physical order, so waveasm.py can rebuild the region
byte-identically (verified by memcmp).

Unreachable islands (cut content) are kept as authoritative DEFB bytes, but
additionally DECODED into `; (dead)` comment lines when they parse cleanly as
code or as an orphaned record block -- so the cut waves are readable.

Usage:  python3 tools/wavedis.py [assets/dgroup.bin] [-o wave/levels.wave]
"""
import struct, sys, os

BASE, END = 0xA0, 0xCB2
DIR_OFF, NDIR = 0x94, 6
NAMES_OFF, NTYPES = 0xCB2, 81
SIZES = {0: 1, 1: 2, 2: 3, 3: 4, 4: 3, 5: 3, 6: 3, 7: 1, 8: 3, 9: 5}
MNEMO = ['CLEAR', 'GETCHAR', 'PUT', 'WHILE', 'GOTO',
         'SETDIST', 'GOSUB', 'RETURN', 'BEQ', 'WAVE']


def load(path):
    blob = open(path, 'rb').read()
    return blob, blob[BASE:END], struct.unpack('<%dH' % NDIR,
                                               blob[DIR_OFF:DIR_OFF + 2 * NDIR])


def type_names(blob):
    """enemy-type symbol table from the 8-char name table @0xCB2."""
    syms, used = {}, {}
    for i in range(NTYPES):
        raw = blob[NAMES_OFF + i * 8:NAMES_OFF + i * 8 + 8]
        nm = raw.decode('ascii', 'replace').strip()
        s = ''.join(c if c.isalnum() else '_' for c in nm)
        while '__' in s:
            s = s.replace('__', '_')
        if not s or s[0].isdigit():
            s = 'T%d' % i
        if s in used:                       # collision -> disambiguate
            s = '%s_T%d' % (s, i)
        used[s] = i
        syms[i] = s
    return syms


def walk(rgn, dirs):
    """reachability: classify every byte; collect targets and record blocks."""
    cls = [None] * len(rgn)                # 'C' code, 'R' record, None unknown
    seen, targets = set(), set()
    put_recs, wave_recs = set(), set()

    def mark(a, b, tag):
        for i in range(a, b):
            assert 0 <= i < len(rgn), 'mark out of region @%#x' % (a + BASE)
            assert cls[i] in (None, tag), \
                'classification conflict @%#x: %s vs %s' % (i + BASE, cls[i], tag)
            cls[i] = tag

    work = list(dirs)
    while work:
        pc = work.pop()
        if pc in seen:
            continue
        assert 0 <= pc < len(rgn), 'flow out of region @%#x' % (pc + BASE)
        seen.add(pc)
        op = rgn[pc]
        assert op <= 9, 'halt byte %#x reached @%#x (unhandled)' % (op, pc + BASE)
        sz = SIZES[op]
        mark(pc, pc + sz, 'C')
        if op in (2, 9):                                    # PUT / WAVE
            rec = struct.unpack('<H', rgn[pc + 1:pc + 3])[0]
            (put_recs if op == 2 else wave_recs).add(rec)
        if op in (3, 4, 6, 8):                              # branches
            toff = pc + 2 if op == 3 else pc + 1
            t = struct.unpack('<H', rgn[toff:toff + 2])[0]
            targets.add(t)
            work.append(t)
        if op not in (4, 7):                                # fallthrough
            work.append(pc + sz)

    recs = {}                                               # off -> count
    for rec in sorted(put_recs | wave_recs):
        cnt = rgn[rec]
        assert 0 < cnt < 128, 'odd record count %d @%#x' % (cnt, rec + BASE)
        mark(rec, rec + 1 + cnt * 5, 'R')
        recs[rec] = cnt
    return cls, seen, targets, recs, put_recs, wave_recs


def islands_of(cls):
    """[(start,end)] of unclassified byte runs."""
    out, i = [], 0
    while i < len(cls):
        if cls[i] is None:
            j = i
            while j < len(cls) and cls[j] is None:
                j += 1
            out.append((i, j))
            i = j
        else:
            i += 1
    return out


def fmt_rec(rgn, off):
    t, dep, y, x, vz = rgn[off:off + 5]
    ts = t if t < 128 else t - 256
    sy = y if y < 128 else y - 256
    sx = x if x < 128 else x - 256
    sv = vz if vz < 128 else vz - 256
    return ts, dep, sy, sx, sv


def decode_dead(rgn, a, b, syms, isl_name):
    """speculatively decode an unreachable island as code OR a record block;
    returns comment lines, or None when it does not parse cleanly."""
    chunk = rgn[a:b]
    # orphaned record block: [count][count x 5-byte records]
    if len(chunk) >= 6 and 0 < chunk[0] < 128 and len(chunk) == 1 + chunk[0] * 5:
        out = ['        ; (dead) RECS %d           '
               '-- orphaned record block (cut content)' % chunk[0]]
        for k in range(chunk[0]):
            ts, dep, sy, sx, sv = fmt_rec(rgn, a + 1 + k * 5)
            out.append('        ; (dead) REC  %-10s %3d, %4d, %4d, %3d'
                       % (syms.get(ts, 'T%d' % ts) + ',', dep, sy, sx, sv))
        return out
    # dead code: decode ops until the island is exactly consumed
    out, i = [], a
    while i < b:
        op = rgn[i]
        if op > 9 or i + SIZES[op] > b:
            return None
        sz = SIZES[op]
        if op == 0:
            txt = 'CLEAR'
        elif op == 1:
            s = rgn[i + 1]
            txt = 'GETCHAR %s' % syms.get(s, 'T%d' % s)
        elif op in (2, 9):
            rec = struct.unpack('<H', rgn[i + 1:i + 3])[0]
            tgt = isl_name.get(rec, '0x%03x' % rec)
            txt = ('PUT  %s' % tgt) if op == 2 else \
                  ('WAVE %s, %d, %d' % (tgt, rgn[i + 3], rgn[i + 4]))
        elif op == 3:
            t = struct.unpack('<H', rgn[i + 2:i + 4])[0]
            txt = 'WHILE %d, %s' % (rgn[i + 1], isl_name.get(t, '0x%03x' % t))
        elif op in (4, 6, 8):
            t = struct.unpack('<H', rgn[i + 1:i + 3])[0]
            txt = '%s %s' % (MNEMO[op], isl_name.get(t, '0x%03x' % t))
        elif op == 5:
            txt = 'SETDIST %d' % struct.unpack('<H', rgn[i + 1:i + 3])[0]
        else:
            txt = 'RETURN'
        out.append('        ; (dead) %-24s ; @%04x' % (txt, i + BASE))
        i += sz
    return out


def main():
    src = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') \
        else 'assets/dgroup.bin'
    out = 'wave/levels.wave'
    if '-o' in sys.argv:
        out = sys.argv[sys.argv.index('-o') + 1]
    blob, rgn, dirs = load(src)
    syms = type_names(blob)
    cls, seen, targets, recs, put_recs, wave_recs = walk(rgn, dirs)
    isl = islands_of(cls)
    isl_name = {a: 'unused_%03x' % a for a, b in isl}

    # ---- symbol assignment ------------------------------------------------
    lab = {}
    for n, d in enumerate(dirs):
        lab[d] = 'script%d' % n
    for t in sorted(targets):
        lab.setdefault(t, 'loc_%03x' % t)
    reclab = {}
    for r in sorted(recs):
        first = rgn[r + 1]
        nm = syms.get(first if first < 128 else first - 256, 'T%d' % first)
        reclab[r] = 'recs_%03x_%s' % (r, nm.lower())

    # ---- emit, physical order --------------------------------------------
    L = []
    L.append('; levels.wave -- Fire & Forget II wave/path-VM scripts,')
    L.append('; disassembled losslessly from DGROUP 0xA0..0xCB2 (dir @0x94).')
    L.append('; Mnemonics = the original tool\'s keyword table (DGROUP @0x355A).')
    L.append(';')
    L.append('; REC fields: type, depth(u8, z ahead of camera), y(i8), x(i8,')
    L.append(';             screen ~= x*25/16), vz(i8, engine adds vz/2).')
    L.append('; WAVE args:  record block, spawn count, z-distance between spawns.')
    L.append('; DEFB     :  bytes never reached by either VM (verbatim);')
    L.append(';             the `(dead)` comments decode them -- CUT CONTENT.')
    L.append('')
    ent = {d: n for n, d in enumerate(dirs)}
    i = 0
    while i < len(rgn):
        at = '        ; @%04x' % (i + BASE)
        if i in ent:
            L.append('')
            L.append('; ======================= SCRIPT %d '
                     '=======================' % ent[i])
            L.append('ENTRY %d' % ent[i])
        if i in lab:
            L.append('%s:%s' % (lab[i], '' if i in ent else at))
        if i in recs:
            cnt = recs[i]
            L.append('%s:%s' % (reclab[i], at))
            use = []
            if i in put_recs:
                use.append('PUT')
            if i in wave_recs:
                use.append('WAVE')
            L.append('        RECS %d              ; used by %s'
                     % (cnt, '+'.join(use)))
            for k in range(cnt):
                ts, dep, sy, sx, sv = fmt_rec(rgn, i + 1 + k * 5)
                L.append('        REC  %-10s %3d, %4d, %4d, %3d'
                         % (syms.get(ts, 'T%d' % ts) + ',', dep, sy, sx, sv))
            i += 1 + cnt * 5
            continue
        if i in seen:
            op = rgn[i]
            if op == 0:
                L.append('        CLEAR%s' % at)
            elif op == 1:
                s = rgn[i + 1]
                L.append('        GETCHAR %-9s%s  ; wait: sprite composed'
                         % (syms.get(s, 'T%d' % s), at))
            elif op == 2:
                rec = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        PUT  %s%s' % (reclab[rec], at))
            elif op == 3:
                cmp8 = rgn[i + 1]
                t = struct.unpack('<H', rgn[i + 2:i + 4])[0]
                L.append('        WHILE %d, %s%s  ; while dist > %d<<4'
                         % (cmp8, lab[t], at, cmp8))
            elif op == 4:
                t = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        GOTO %s%s' % (lab[t], at))
            elif op == 5:
                v = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        SETDIST %d%s' % (v, at))
            elif op == 6:
                t = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        GOSUB %s%s' % (lab[t], at))
            elif op == 7:
                L.append('        RETURN%s' % at)
            elif op == 8:
                t = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        BEQ  %s%s  ; branch when dist == 0'
                         % (lab[t], at))
            elif op == 9:
                rec = struct.unpack('<H', rgn[i + 1:i + 3])[0]
                L.append('        WAVE %s, %d, %d%s'
                         % (reclab[rec], rgn[i + 3], rgn[i + 4], at))
            i += SIZES[op]
            continue
        # unknown island -> DEFB (authoritative) + decoded (dead) comments
        j = i
        while j < len(rgn) and cls[j] is None and j not in ent:
            j += 1
        chunk = rgn[i:j]
        L.append('%s:%s  ; unreachable (cut content), kept verbatim'
                 % (isl_name[i], at))
        dead = decode_dead(rgn, i, j, syms,
                           {**reclab, **lab, **isl_name})
        if dead:
            L.extend(dead)
        for k in range(0, len(chunk), 12):
            L.append('        DEFB %s'
                     % ','.join('0x%02X' % b for b in chunk[k:k + 12]))
        i = j

    os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
    open(out, 'w').write('\n'.join(L) + '\n')
    unk = cls.count(None)
    print('%s: %d lines; region 0x%x bytes = code %d + records %d + DEFB %d'
          % (out, len(L), len(rgn), cls.count('C'), cls.count('R'), unk))


if __name__ == '__main__':
    main()
