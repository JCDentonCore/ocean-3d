#!/usr/bin/env python3
"""Detect fish clusters in ocean.png (silver-blue blobs under the horizon).

Fish are silver (R/G > ~0.6); water is blue-green (R/G < ~0.35). Restrict to
the underwater region (below the horizon) to skip the sky.
"""
import sys, zlib, struct

def load_png(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    idat = b''
    w = h = None
    while pos < len(data):
        ln = struct.unpack('>I', data[pos:pos+4])[0]
        typ = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+ln]
        if typ == b'IHDR':
            w, h = struct.unpack('>II', chunk[:8])
        elif typ == b'IDAT':
            idat += chunk
        pos += 12 + ln
    raw = zlib.decompress(idat)
    stride = w * 4
    px = bytearray(w * h * 4)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if f == 0:
            pass
        elif f == 1:  # Sub
            for i in range(stride):
                line[i] = (line[i] + (line[i-4] if i >= 4 else 0)) & 0xFF
        elif f == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:  # Average
            for i in range(stride):
                a = line[i-4] if i >= 4 else 0
                line[i] = (line[i] + (a + prev[i]) // 2) & 0xFF
        elif f == 4:  # Paeth
            for i in range(stride):
                a = line[i-4] if i >= 4 else 0
                b = prev[i]
                c = prev[i-4] if i >= 4 else 0
                pa, pb, pc = abs(b-c), abs(a-b), abs(a+b-2*c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        prev = line
        px[y*stride:(y+1)*stride] = line
    return w, h, px

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'ocean.png'
    w, h, px = load_png(path)
    # horizon ~ 40% down (cam y=8, pitch=-0.1); use 45% to stay clear
    y0 = int(h * 0.45)
    mask = bytearray(w * h)
    for y in range(y0, h):
        for x in range(w):
            i = (y * w + x) * 4
            r, g, b = px[i], px[i+1], px[i+2]
            if g > 30 and r > 20 and (r * 100) // max(g, 1) > 55 and r < 240:
                mask[y * w + x] = 1
    # connected components (4-conn), BFS
    seen = bytearray(w * h)
    comps = []
    for y in range(y0, h):
        row = y * w
        for x in range(w):
            i = row + x
            if mask[i] and not seen[i]:
                stack = [(x, y)]
                seen[i] = 1
                n = 0; minx = maxx = x; miny = maxy = y
                while stack:
                    cx, cy = stack.pop()
                    n += 1
                    minx = min(minx, cx); maxx = max(maxx, cx)
                    miny = min(miny, cy); maxy = max(maxy, cy)
                    for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
                        nx, ny = cx+dx, cy+dy
                        if 0 <= nx < w and y0 <= ny < h:
                            j = ny * w + nx
                            if mask[j] and not seen[j]:
                                seen[j] = 1
                                stack.append((nx, ny))
                if n >= 15:
                    comps.append((n, maxx-minx+1, maxy-miny+1, minx, miny))
    comps.sort(reverse=True)
    print(f"image {w}x{h}, underwater region y>{y0}")
    print(f"fish clusters (>=15 px): {len(comps)}")
    for n, cw, chh, mx, my in comps[:15]:
        print(f"  size={n:5d}px  bbox={cw}x{chh}  at ({mx},{my})")
    tot = sum(c[0] for c in comps)
    print(f"total fish px: {tot}")

if __name__ == '__main__':
    main()
