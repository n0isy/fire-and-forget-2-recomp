#!/usr/bin/env python3
"""waveasm.py -- assembler for the Fire & Forget II wave-VM DSL (levels.wave)
back into the binary bytecode region (DGROUP 0xA0..0xCB2) + the 6-entry
script directory (@0x94). Inverse of wavedis.py.

Two-pass: pass 1 sizes every statement and binds labels (label value = the
absolute operand encoding = offset within the region, i.e. address-0xA0);
pass 2 emits bytes.

Usage:
    python3 tools/waveasm.py wave/levels.wave -o /tmp/wave.bin
    python3 tools/waveasm.py wave/levels.wave --verify assets/dgroup.bin
    python3 tools/waveasm.py wave/levels.wave --c /tmp/wave_data.c
"""
import struct, sys, re, os

BASE, END = 0xA0, 0xCB2
DIR_OFF, NDIR = 0x94, 6
NAMES_OFF, NTYPES = 0xCB2, 81
OPS = {'CLEAR': (0, 1), 'GETCHAR': (1, 2), 'PUT': (2, 3), 'WHILE': (3, 4),
       'GOTO': (4, 3), 'SETDIST': (5, 3), 'GOSUB': (6, 3), 'RETURN': (7, 1),
       'BEQ': (8, 3), 'WAVE': (9, 5)}


def type_syms(blob):
    """the same enemy-symbol derivation as wavedis.py (must match exactly)."""
    sym2idx, used = {}, {}
    for i in range(NTYPES):
        raw = blob[NAMES_OFF + i * 8:NAMES_OFF + i * 8 + 8]
        nm = raw.decode('ascii', 'replace').strip()
        s = ''.join(c if c.isalnum() else '_' for c in nm)
        while '__' in s:
            s = s.replace('__', '_')
        if not s or s[0].isdigit():
            s = 'T%d' % i
        if s in used:
            s = '%s_T%d' % (s, i)
        used[s] = i
        sym2idx[s] = i
    return sym2idx


def parse(path):
    """-> list of (kind, args, lineno); kinds: label/entry/op/recs/rec/defb"""
    stmts = []
    for ln, raw in enumerate(open(path), 1):
        line = raw.split(';', 1)[0].rstrip()
        if not line.strip():
            continue
        s = line.strip()
        m = re.match(r'^([A-Za-z_]\w*):$', s)
        if m:
            stmts.append(('label', m.group(1), ln))
            continue
        m = re.match(r'^ENTRY\s+(\d+)$', s)
        if m:
            stmts.append(('entry', int(m.group(1)), ln))
            continue
        parts = s.split(None, 1)
        op = parts[0].upper()
        args = [a.strip() for a in parts[1].split(',')] if len(parts) > 1 else []
        if op == 'RECS':
            stmts.append(('recs', int(args[0], 0), ln))
        elif op == 'REC':
            stmts.append(('rec', args, ln))
        elif op == 'DEFB':
            stmts.append(('defb', [int(a, 0) for a in args], ln))
        elif op in OPS:
            stmts.append(('op', (op, args), ln))
        else:
            raise SystemExit('%s:%d: unknown statement %r' % (path, ln, s))
    return stmts


def main():
    src = sys.argv[1]
    blob = None
    for flag in ('--verify', '--blob'):
        if flag in sys.argv:
            blob = open(sys.argv[sys.argv.index(flag) + 1], 'rb').read()
    if blob is None:                       # symbols need the name table
        blob = open('assets/dgroup.bin', 'rb').read()
    sym2idx = type_syms(blob)
    stmts = parse(src)

    # ---- pass 1: layout ---------------------------------------------------
    labels, entries = {}, {}
    pos = 0
    for kind, a, ln in stmts:
        if kind == 'label':
            assert a not in labels, 'dup label %s (line %d)' % (a, ln)
            labels[a] = pos
        elif kind == 'entry':
            entries[a] = pos
        elif kind == 'op':
            pos += OPS[a[0]][1]
        elif kind == 'recs':
            pos += 1
        elif kind == 'rec':
            pos += 5
        elif kind == 'defb':
            pos += len(a)
    size = pos

    def val(tok, ln):
        if tok in labels:
            return labels[tok]
        if tok in sym2idx:
            return sym2idx[tok]
        try:
            return int(tok, 0)
        except ValueError:
            raise SystemExit('line %d: unresolved symbol %r' % (ln, tok))

    # ---- pass 2: emit -----------------------------------------------------
    out = bytearray()
    for kind, a, ln in stmts:
        if kind in ('label', 'entry'):
            continue
        if kind == 'op':
            op, args = a
            code, sz = OPS[op]
            out.append(code)
            if op == 'GETCHAR':
                out.append(val(args[0], ln) & 0xFF)
            elif op in ('PUT', 'GOTO', 'GOSUB', 'BEQ'):
                out += struct.pack('<H', val(args[0], ln))
            elif op == 'SETDIST':
                out += struct.pack('<H', val(args[0], ln))
            elif op == 'WHILE':
                out.append(val(args[0], ln) & 0xFF)
                out += struct.pack('<H', val(args[1], ln))
            elif op == 'WAVE':
                out += struct.pack('<H', val(args[0], ln))
                out.append(val(args[1], ln) & 0xFF)
                out.append(val(args[2], ln) & 0xFF)
        elif kind == 'recs':
            out.append(a & 0xFF)
        elif kind == 'rec':
            t = val(a[0], ln)
            dep, y, x, vz = (int(v, 0) for v in a[1:5])
            out += bytes(b & 0xFF for b in (t, dep, y, x, vz))
        elif kind == 'defb':
            out += bytes(b & 0xFF for b in a)
    assert len(out) == size

    dirw = struct.pack('<%dH' % NDIR,
                       *[entries[n] for n in range(NDIR)]) \
        if len(entries) == NDIR else None

    # ---- outputs / verification -------------------------------------------
    if '-o' in sys.argv:
        open(sys.argv[sys.argv.index('-o') + 1], 'wb').write(out)
    if '--c' in sys.argv:
        cpath = sys.argv[sys.argv.index('--c') + 1]
        hpath = cpath[:-2] + '.h' if cpath.endswith('.c') else cpath + '.h'
        hname = os.path.basename(hpath)
        gen = ('/* %%s -- GENERATED by tools/waveasm.py from %s; DO NOT EDIT.\n'
               ' * The wave/path-VM bytecode region (DGROUP 0xA0..0x%X) and\n'
               ' * the 6-entry script directory (DGROUP 0x94), rebuilt from\n'
               ' * the DSL source. Byte-identity vs the loaded blob is checked\n'
               ' * at boot (wave_data_check, game/game_main.c). */\n\n'
               % (src, BASE + size))
        with open(cpath, 'w') as f:
            f.write(gen % os.path.basename(cpath))
            f.write('#include "%s"\n\n' % hname)
            f.write('const unsigned char wave_bytecode[WAVE_BYTECODE_LEN]'
                    ' = {\n')
            for i in range(0, size, 12):
                f.write('    %s,\n' % ','.join('0x%02X' % b
                                               for b in out[i:i + 12]))
            f.write('};\n')
            if dirw:
                f.write('const unsigned short wave_script_dir[%d] = '
                        '{ %s };\n' % (NDIR, ', '.join(
                            '0x%04X' % entries[n] for n in range(NDIR))))
        with open(hpath, 'w') as f:
            f.write(gen % hname)
            f.write('#ifndef WAVE_DATA_H\n#define WAVE_DATA_H\n\n')
            f.write('#define WAVE_BYTECODE_BASE 0x%X   '
                    '/* DGROUP offset of the region */\n' % BASE)
            f.write('#define WAVE_BYTECODE_LEN  0x%X\n' % size)
            f.write('#define WAVE_DIR_BASE      0x%X\n' % DIR_OFF)
            f.write('#define WAVE_DIR_N         %d\n\n' % NDIR)
            f.write('extern const unsigned char  '
                    'wave_bytecode[WAVE_BYTECODE_LEN];\n')
            f.write('extern const unsigned short wave_script_dir[WAVE_DIR_N];'
                    '\n\n#endif\n')
    if '--verify' in sys.argv:
        ref = blob[BASE:END]
        ok = bytes(out) == ref
        print('region : %d bytes assembled, ref %d bytes -> %s'
              % (len(out), len(ref), 'MATCH' if ok else 'MISMATCH'))
        if not ok:
            n = min(len(out), len(ref))
            for i in range(n):
                if out[i] != ref[i]:
                    print('  first diff @%#x (file 0x%x): asm %02x != ref %02x'
                          % (i + BASE, i, out[i], ref[i]))
                    break
            sys.exit(1)
        if dirw:
            okd = dirw == blob[DIR_OFF:DIR_OFF + 2 * NDIR]
            print('dir    : %s' % ('MATCH' if okd else 'MISMATCH'))
            if not okd:
                sys.exit(1)
    print('OK: %d bytes' % len(out))


if __name__ == '__main__':
    main()
