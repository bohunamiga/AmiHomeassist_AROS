#!/usr/bin/env python3
"""Macht aus dem Home-Assistant-Logo ein Amiga-Icon und die Bilddaten fuers
Fenster.

Zwei Fassungen in einer Datei, wie es auf dem Amiga ueblich ist:

  klassisch   48x48 in den vier Workbench-Grundfarben (grau, schwarz, weiss,
              blau). Sieht auf jeder Kiste richtig aus, auch mit alter
              icon.library, und hat keine Transparenz - Farbe 0 ist der
              Hintergrund.

  GlowIcon    dieselbe Zeichnung in 16 Farben mit echter Transparenz und
              einem Lichtkranz im angewaehlten Zustand. icon.library ab
              Version 44 zeigt diese und ignoriert die klassische.

Das Fensterlogo benutzt die 16-Farben-Fassung als Bodychunk.

    python3 appicon.py            schreibt AmiHomeassist.info und logo.c
"""

import struct
import sys

from PIL import Image

# Quellbild: das Home-Assistant-Logo als WebP, danebengelegt.
SRC = "HA_Logo.webp"
SIZE = 48          # Workbench-Icon
LOGO_SIZE = 72     # das Logo im Fenster
NOPOS = -2147483648

# Die vier Workbench-Grundfarben fuer die klassische Fassung.
WB_GREY, WB_BLACK, WB_WHITE, WB_BLUE = 0, 1, 2, 3

# 16 Farben fuer GlowIcon und Fensterlogo. 0 ist durchsichtig.
GLOW_PAL = [
    (0x95, 0x95, 0x95),   # 0  durchsichtig / Hintergrund
    (0x00, 0x2A, 0x3A),   # 1  sehr dunkles Blau, Kante
    (0x0A, 0x5E, 0x7E),   # 2
    (0x10, 0x8A, 0xB4),   # 3
    (0x18, 0xBC, 0xF2),   # 4  das Blau des Logos
    (0x5A, 0xD2, 0xF6),   # 5
    (0x9C, 0xE6, 0xFA),   # 6
    (0xFF, 0xFF, 0xFF),   # 7  die Leitungen
    (0xE0, 0xF6, 0xFD),   # 8
    (0x30, 0x30, 0x30),   # 9  Schatten
    (0x60, 0x60, 0x60),   # 10
    (0xB0, 0xB0, 0xB0),   # 11
    (0xD8, 0xD8, 0xD8),   # 12
    (0x18, 0x9C, 0xC8),   # 13
    (0x74, 0xDC, 0xF8),   # 14
    (0xFF, 0xE0, 0x80),   # 15 Lichtkranz
]
GLOW_T = 0
GLOW_GLOW = 15


def load(size):
    im = Image.open(SRC).convert("RGBA")
    return im.resize((size, size), Image.LANCZOS)


def nearest(rgb, pal, skip0=True):
    best, bestd = 1 if skip0 else 0, 1 << 30
    for i, c in enumerate(pal):
        if skip0 and i == 0:
            continue
        d = (c[0] - rgb[0]) ** 2 + (c[1] - rgb[1]) ** 2 + (c[2] - rgb[2]) ** 2
        if d < bestd:
            best, bestd = i, d
    return best


def grids(im):
    """Liefert (klassisch 4 Farben, glow 16 Farben)."""
    size = im.size[0]
    px = im.load()
    classic = [[WB_GREY] * size for _ in range(size)]
    glow = [[GLOW_T] * size for _ in range(size)]

    for y in range(size):
        for x in range(size):
            r, g, b, a = px[x, y]
            if a < 96:
                continue                      # Hintergrund bleibt frei
            glow[y][x] = nearest((r, g, b), GLOW_PAL)

            # Klassisch: hell wird weiss, blau wird blau, dunkel wird schwarz.
            if r > 200 and g > 200 and b > 200:
                classic[y][x] = WB_WHITE
            elif b > 120 and b > r + 40:
                classic[y][x] = WB_BLUE
            else:
                classic[y][x] = WB_BLACK
    return classic, glow


def glow_selected(gn):
    """Angewaehlt: derselbe Umriss mit Lichtkranz - jeder freie Punkt, der die
    Zeichnung beruehrt, leuchtet auf."""
    out = [row[:] for row in gn]
    for y in range(SIZE):
        for x in range(SIZE):
            if gn[y][x] != GLOW_T:
                continue
            if any(gn[y + dy][x + dx] != GLOW_T
                   for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                   if 0 <= y + dy < SIZE and 0 <= x + dx < SIZE):
                out[y][x] = GLOW_GLOW
    return out


def rle_pack(values, depth):
    """ByteRun1 ueber einen Bitstrom: 8-Bit-Steuerbytes, Werte depth Bit breit."""
    stream = []
    i, n = 0, len(values)
    while i < n:
        run = 1
        while i + run < n and values[i + run] == values[i] and run < 128:
            run += 1
        if run >= 2:
            stream.append((257 - run, 8))
            stream.append((values[i], depth))
            i += run
        else:
            lits = []
            while i < n and len(lits) < 128:
                if i + 1 < n and values[i + 1] == values[i]:
                    break
                lits.append(values[i])
                i += 1
            stream.append((len(lits) - 1, 8))
            stream += [(v, depth) for v in lits]

    buf, acc, nbits = bytearray(), 0, 0
    for v, w in stream:
        acc = (acc << w) | (v & ((1 << w) - 1))
        nbits += w
        while nbits >= 8:
            nbits -= 8
            buf.append((acc >> nbits) & 0xFF)
    if nbits:
        buf.append((acc << (8 - nbits)) & 0xFF)
    return bytes(buf)


def imag_chunk(grid, pal):
    depth = max(1, (len(pal) - 1).bit_length())
    img = rle_pack([p for row in grid for p in row], depth)
    palbytes = bytes(c for rgb in pal for c in rgb)
    body = struct.pack(">BBBBBBHH", 0, len(pal) - 1, 0x01 | 0x02, 1, 0,
                       depth, len(img) - 1, len(palbytes) - 1)
    body += img + palbytes
    return (b"IMAG" + struct.pack(">I", len(body)) + body
            + (b"\0" if len(body) & 1 else b""))


def glow_form(glow):
    face = struct.pack(">BBBBH", SIZE - 1, SIZE - 1, 1, 0x11,
                       len(GLOW_PAL) * 3 - 1)
    body = (b"ICON"
            + b"FACE" + struct.pack(">I", len(face)) + face
            + imag_chunk(glow, GLOW_PAL)
            + imag_chunk(glow_selected(glow), GLOW_PAL))
    return b"FORM" + struct.pack(">I", len(body)) + body


def planar(g, depth):
    """Klassische Icons: Plane fuer Plane, jede Zeile wortweise."""
    words = (SIZE + 15) // 16
    out = bytearray()
    for plane in range(depth):
        for y in range(SIZE):
            bits = 0
            for x in range(SIZE):
                if (g[y][x] >> plane) & 1:
                    bits |= 1 << (words * 16 - 1 - x)
            out += bits.to_bytes(words * 2, "big")
    return bytes(out)


def image_header(depth):
    return struct.pack(">hhhhhIBBI", 0, 0, SIZE, SIZE, depth, 1, 0x03, 0x00, 0)


def build_info(classic, glow, kind="tool"):
    do_type = {"drawer": 2, "tool": 3, "project": 4}[kind]

    gadget = struct.pack(">IhhhhHHH", 0, 0, 0, SIZE, SIZE, 0x0006, 0x0001, 0x0001)
    gadget += struct.pack(">IIIIIHI", 1, 1, 0, 0, 0, 0, 0)
    gadget = gadget[:44]

    do = struct.pack(">HH", 0xE310, 1) + gadget
    do += bytes([do_type, 0])
    do += struct.pack(">II", 0, 0)
    do += struct.pack(">ii", NOPOS, NOPOS)
    do += struct.pack(">III", 1 if kind == "drawer" else 0, 0, 0)
    assert len(do) == 78, len(do)

    out = bytearray(do)
    if kind == "drawer":
        nw = struct.pack(">hhhhBBIIIIIIhhhhH", 60, 50, 320, 130, 0xFF, 0xFF,
                         0, 0, 0, 0, 0, 0, 90, 40, 640, 200, 1)
        out += (nw[:48] + struct.pack(">ii", 0, 0) + b"\0" * 56)[:56]

    out += image_header(2) + planar(classic, 2)
    out += image_header(2) + planar(classic, 2)
    out += glow_form(glow)
    return bytes(out)


def emit_logo_c(glow):
    """Bodychunk-Daten fuers Fenster, 16 Farben, Farbe 0 durchsichtig."""
    size = len(glow)
    words = (size + 15) // 16
    stride = words * 2
    body = bytearray()
    for row in glow:
        for plane in range(4):
            bits = bytearray(stride)
            for x, idx in enumerate(row):
                if (idx >> plane) & 1:
                    bits[x // 8] |= 0x80 >> (x % 8)
            body += bits

    lines = ["/* Erzeugt von appicon.py aus HA_Logo.webp.",
             " * Home Assistant ist ein eigenstaendiges Projekt;",
             " * das Logo gehoert dessen Urhebern. */",
             "",
             '#include "icons.h"',
             "",
             "const ULONG logo_colors[%d] = {" % (len(GLOW_PAL) * 3)]
    for r, g, b in GLOW_PAL:
        lines.append("    0x%08lXUL, 0x%08lXUL, 0x%08lXUL,"
                     % (r * 0x01010101, g * 0x01010101, b * 0x01010101))
    lines.append("};")
    lines.append("")
    lines.append("const UBYTE logo_body[%d] = {" % len(body))
    for i in range(0, len(body), 12):
        lines.append("    " + " ".join("0x%02X," % v for v in body[i:i + 12]))
    lines.append("};")
    lines.append("")
    lines.append("const struct IconDef logo_image = "
                 "{ logo_body, %d, %d, 4 };" % (size, size))
    return "\n".join(lines) + "\n"


def main():
    classic, glow = grids(load(SIZE))

    with open("AmiHomeassist.info", "wb") as f:
        f.write(build_info(classic, glow, "tool"))
    with open("Drawer.info", "wb") as f:
        f.write(build_info(classic, glow, "drawer"))

    # Das Fensterlogo eigenstaendig, in seiner eigenen Groesse
    _, glow_big = grids(load(LOGO_SIZE))
    with open("logo.c", "w") as f:
        f.write(emit_logo_c(glow_big))

    # Vorschau, damit man vor dem Uebertragen sieht, was herauskommt
    prev = Image.new("RGB", (SIZE * 2 * 6, SIZE * 6), (0x95, 0x95, 0x95))
    px = prev.load()
    for n, g in enumerate((classic, glow)):
        pal = [(0x95, 0x95, 0x95), (0, 0, 0), (255, 255, 255),
               (0x3B, 0x67, 0xA2)] if n == 0 else GLOW_PAL
        for y in range(SIZE):
            for x in range(SIZE):
                c = pal[g[y][x]]
                for dy in range(6):
                    for dx in range(6):
                        px[(n * SIZE + x) * 6 + dx, y * 6 + dy] = c
    prev.save("/tmp/appicon_preview.png")
    print("AmiHomeassist.info, Drawer.info, logo.c und die Vorschau sind da")


if __name__ == "__main__":
    main()
