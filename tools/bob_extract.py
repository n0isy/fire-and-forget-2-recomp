#!/usr/bin/env python3
"""
bob_extract.py  --  Decode the Fire & Forget II sprite bank BOB.CPT (BOB = Titus
Blitter OBjects) and dump individual sprites to PNG using the default EGA-16
palette.

Format (reverse engineered from FF2EGA.EXE seg 0A0D / 1187; see
docs/bob_format.md):

  file:
    +0x00  u16  sprite_count                (=236 for BOB.bin)
    +0x02  sprite[0]
           sprite[1] ...                    packed back to back, no gap

  sprite record (little-endian):
    +0x00  u16  height   (rows)             ([ptr-4] in the engine)
    +0x02  u16  width    (pixels, mult. 16) ([ptr-2] in the engine)
    +0x04  planar pixel data, length = (width/2)*height bytes
             4 consecutive EGA bit-planes, plane0 first.
             plane size = height * (width/8) bytes.
             within a plane: row r = (width/8) bytes, MSB = leftmost pixel.
           colour(x,y) = p0bit | p1bit<<1 | p2bit<<2 | p3bit<<3

The directory builder fn0A0D_09E4 stores, per sprite, a far pointer to the pixel
data (+0x04) and reads the two dim words from [ptr-4]/[ptr-2]; it advances to the
next sprite by ((width>>2)*height + 2) << 1 == 4 + (width/2)*height bytes.
"""

import os
import sys
import struct
import zlib

# Default IBM EGA 16-colour palette (RGB).
EGA16 = [
    (0x00, 0x00, 0x00),  # 0  black
    (0x00, 0x00, 0xAA),  # 1  blue
    (0x00, 0xAA, 0x00),  # 2  green
    (0x00, 0xAA, 0xAA),  # 3  cyan
    (0xAA, 0x00, 0x00),  # 4  red
    (0xAA, 0x00, 0xAA),  # 5  magenta
    (0xAA, 0x55, 0x00),  # 6  brown
    (0xAA, 0xAA, 0xAA),  # 7  light gray
    (0x55, 0x55, 0x55),  # 8  dark gray
    (0x55, 0x55, 0xFF),  # 9  light blue
    (0x55, 0xFF, 0x55),  # 10 light green
    (0x55, 0xFF, 0xFF),  # 11 light cyan
    (0xFF, 0x55, 0x55),  # 12 light red
    (0xFF, 0x55, 0xFF),  # 13 light magenta
    (0xFF, 0xFF, 0x55),  # 14 yellow
    (0xFF, 0xFF, 0xFF),  # 15 white
]


def u16(buf, off):
    return struct.unpack_from("<H", buf, off)[0]


def parse_bank(buf):
    """Return (count, [ (index, rec_off, height, width, data_off, data_len) ])."""
    count = u16(buf, 0)
    recs = []
    p = 2
    for i in range(count):
        if p + 4 > len(buf):
            raise ValueError("record header overflow at sprite %d (off 0x%x)" % (i, p))
        h = u16(buf, p)
        w = u16(buf, p + 2)
        dlen = (w >> 1) * h           # (width/2)*height
        doff = p + 4
        if doff + dlen > len(buf):
            raise ValueError("pixel data overflow at sprite %d" % i)
        recs.append((i, p, h, w, doff, dlen))
        p = doff + dlen
    return count, recs, p


def decode_sprite(buf, h, w, doff):
    """Decode one planar sprite into a list of rows; each row is a list of 4-bit
    colour indices (0..15)."""
    row_bytes = w >> 3                 # bytes per plane row
    plane_size = row_bytes * h         # bytes per plane
    px = [[0] * w for _ in range(h)]
    for plane in range(4):
        base = doff + plane * plane_size
        bit = 1 << plane
        for y in range(h):
            ro = base + y * row_bytes
            row = px[y]
            for bx in range(row_bytes):
                b = buf[ro + bx]
                if not b:
                    continue
                x0 = bx << 3
                for k in range(8):
                    if b & (0x80 >> k):
                        row[x0 + k] |= bit
    return px


def write_png(path, px, h, w, scale=1, transparent0=True):
    """Write an RGBA PNG from index rows `px` using the EGA-16 palette."""
    pal = [(r, g, b, 255) for (r, g, b) in EGA16]
    if transparent0:
        pal[0] = (0, 0, 0, 0)

    raw = bytearray()
    for y in range(h):
        row = px[y]
        line = bytearray()
        line.append(0)  # filter: none
        for x in range(w):
            r, g, b, a = pal[row[x]]
            cell = bytes((r, g, b, a))
            line.extend(cell * scale)
        rowbytes = bytes(line[1:])
        for _ in range(scale):
            raw.append(0)
            raw.extend(rowbytes)

    W, H = w * scale, h * scale

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)  # 8-bit RGBA
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def make_contact_sheet(path, buf, recs, cols=16, cell=None, scale=1, bg=5):
    """Lay every sprite onto a grid; transparent pixels show palette colour `bg`."""
    n = len(recs)
    rows = (n + cols - 1) // cols
    if cell is None:
        cw = max(r[3] for r in recs)
        ch = max(r[2] for r in recs)
    else:
        cw, ch = cell
    pad = 2
    cwp, chp = cw + pad, ch + pad
    W = cols * cwp
    H = rows * chp
    canvas = [[bg] * W for _ in range(H)]
    for (i, roff, h, w, doff, dlen) in recs:
        gx = (i % cols) * cwp + pad // 2
        gy = (i // cols) * chp + pad // 2
        spr = decode_sprite(buf, h, w, doff)
        for y in range(min(h, ch)):
            for x in range(min(w, cw)):
                c = spr[y][x]
                if c:
                    canvas[gy + y][gx + x] = c
    write_png(path, canvas, H, W, scale=scale, transparent0=False)


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Extract BOB.CPT sprites to PNG.")
    ap.add_argument("bank", nargs="?",
                    default=os.path.join(os.path.dirname(__file__), "..",
                                         "assets", "unpacked", "BOB.bin"))
    ap.add_argument("-o", "--outdir",
                    default=os.path.join(os.path.dirname(__file__), "..",
                                         "assets", "bob"))
    ap.add_argument("-s", "--scale", type=int, default=1)
    ap.add_argument("--indices", default="",
                    help="comma list of sprite indices to dump (default: a sample)"
                         "  e.g. 0,1,2,54,55 (54/55 = GAME/OVER text)")
    ap.add_argument("--all", action="store_true", help="dump every sprite")
    ap.add_argument("--sheet", action="store_true", help="also make contact sheet")
    ap.add_argument("--list", action="store_true", help="print the directory and exit")
    args = ap.parse_args()

    buf = open(args.bank, "rb").read()
    count, recs, end = parse_bank(buf)
    sys.stderr.write("BOB bank: %d sprites, %d bytes, walk ends at 0x%x (file 0x%x)\n"
                     % (count, len(buf), end, len(buf)))

    if args.list:
        for (i, roff, h, w, doff, dlen) in recs:
            print("%3d  off=0x%05x  %3dx%-3d  data=0x%05x len=%d"
                  % (i, roff, w, h, doff, dlen))
        return

    os.makedirs(args.outdir, exist_ok=True)

    if args.all:
        sel = [r[0] for r in recs]
    elif args.indices:
        sel = [int(x, 0) for x in args.indices.split(",")]
    else:
        # a representative sample: first ship frames + some mid/large objects
        sel = [0, 1, 2, 3, 4, 8, 9, 10, 20, 40, 80, 120, 160, 200, 232, 234, 235]

    for i in sel:
        (idx, roff, h, w, doff, dlen) = recs[i]
        spr = decode_sprite(buf, h, w, doff)
        name = "bob_%03d_%dx%d.png" % (idx, w, h)
        write_png(os.path.join(args.outdir, name), spr, h, w, scale=args.scale)
    sys.stderr.write("wrote %d sprite PNG(s) to %s\n" % (len(sel), args.outdir))

    if args.sheet:
        make_contact_sheet(os.path.join(args.outdir, "_contact_sheet.png"),
                           buf, recs, cols=16, scale=1)
        sys.stderr.write("wrote contact sheet\n")


if __name__ == "__main__":
    main()
