#!/usr/bin/env python3
"""Headless PowerPacker PP20 decruncher.
Port of the ppDecrunch algorithm from libxmp (amigadepack, public domain).
Usage: pp20_unpack.py <in.CPT> <out.bin>
"""
import sys

def pp_decrunch(data: bytes) -> bytes:
    if data[:4] != b'PP20':
        raise ValueError("not a PP20 file")
    offset_lens = data[4:8]                       # 4 "efficiency" bytes
    dest_len = (data[-4] << 16) | (data[-3] << 8) | data[-2]
    skip_bits = data[-1]
    src = data[8:-4]                              # compressed stream (len-12 bytes)

    out = bytearray(dest_len)
    o = dest_len                                  # write backwards: out[--o]
    pos = len(src)                               # read backwards: src[--pos]
    bit_buffer = 0
    bits_left = 0

    def read_bits(n):
        nonlocal pos, bit_buffer, bits_left
        while bits_left < n:
            pos -= 1
            if pos < 0:
                raise ValueError("out of source bits")
            bit_buffer |= src[pos] << bits_left
            bits_left += 8
        val = 0
        bits_left -= n
        for _ in range(n):
            val = (val << 1) | (bit_buffer & 1)
            bit_buffer >>= 1
        return val

    read_bits(skip_bits)                          # skip bits at the start

    written = 0
    while written < dest_len:
        if read_bits(1) == 0:                     # literal run
            todo = 1
            while True:
                x = read_bits(2)
                todo += x
                if x != 3:
                    break
            while todo > 0:
                o -= 1
                out[o] = read_bits(8)
                written += 1
                todo -= 1
            if written == dest_len:
                break
        # match
        x = read_bits(2)
        offbits = offset_lens[x]
        todo = x + 2
        if x == 3:
            if read_bits(1) == 0:
                offbits = 7
            offset = read_bits(offbits)
            while True:
                x = read_bits(3)
                todo += x
                if x != 7:
                    break
        else:
            offset = read_bits(offbits)
        if o + offset >= dest_len:
            raise ValueError("match overflow")
        while todo > 0:
            out[o - 1] = out[o + offset]
            o -= 1
            written += 1
            todo -= 1
    return bytes(out)

if __name__ == "__main__":
    data = open(sys.argv[1], 'rb').read()
    out = pp_decrunch(data)
    open(sys.argv[2], 'wb').write(out)
    print(f"{sys.argv[1]}: {len(data)} -> {len(out)} bytes")
