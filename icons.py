#!/usr/bin/env python3
"""Erzeugt icons.c aus ASCII-Zeichnungen.

Die Bilder von Hand als Hex-Tabellen zu tippen waere unlesbar und kaum zu
korrigieren. Hier stehen sie als Zeichnung, und das Skript macht daraus die
ILBM-BODY-Daten, die MUIs Bodychunk-Klasse erwartet: pro Zeile erst Bitplane 0,
dann Bitplane 1, jede Zeile auf ganze Worte aufgefuellt.

    python3 icons.py > icons.c
"""

# '.' durchsichtig, '#' dunkel, 'g' gruen, 'a' bernstein
PALETTE = [(0x00, 0x00, 0x00), (0x20, 0x20, 0x20),
           (0x22, 0xAA, 0x44), (0xFF, 0xBB, 0x00)]
INDEX = {'.': 0, '#': 1, 'g': 2, 'a': 3}

SWITCH_OFF = [
    "..##########..",
    ".#..........#.",
    "#####........#",
    "#####........#",
    "#####........#",
    "#####........#",
    "#####........#",
    ".#..........#.",
    "..##########..",
]

SWITCH_ON = [
    "..##########..",
    ".#gggggggggg#.",
    "#ggggggggg####",
    "#ggggggggg####",
    "#ggggggggg####",
    "#ggggggggg####",
    "#ggggggggg####",
    ".#gggggggggg#.",
    "..##########..",
]

LAMP = [
    "....####....",
    "..##aaaa##..",
    ".#aaaaaaaa#.",
    ".#aaaaaaaa#.",
    ".#aaaaaaaa#.",
    ".#aaaaaaaa#.",
    "..#aaaaaa#..",
    "...#aaaa#...",
    "....####....",
    "....#..#....",
    "....####....",
    "....#..#....",
]

PLUG = [
    "....####....",
    "....#..#....",
    "..########..",
    ".##########.",
    ".##########.",
    ".###....###.",
    ".##.####.##.",
    ".##.####.##.",
    ".###....###.",
    ".##########.",
    "..########..",
    "............",
]

CHECK_OFF = [
    "############",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "#..........#",
    "############",
]

CHECK_ON = [
    "############",
    "#..........#",
    "#........gg#",
    "#.......gg.#",
    "#.g....gg..#",
    "#.gg..gg...#",
    "#..gggg....#",
    "#...gg.....#",
    "#..........#",
    "#..........#",
    "############",
]

IMAGES = [
    ("switch_off", SWITCH_OFF),
    ("switch_on", SWITCH_ON),
    ("lamp", LAMP),
    ("plug", PLUG),
    ("check_off", CHECK_OFF),
    ("check_on", CHECK_ON),
]

DEPTH = 2


def planar(art):
    """ASCII-Zeichnung -> ILBM-BODY, Zeile fuer Zeile, Plane fuer Plane."""
    height = len(art)
    width = len(art[0])
    for row in art:
        if len(row) != width:
            raise SystemExit("Zeile mit abweichender Breite: %r" % row)

    words = (width + 15) // 16
    stride = words * 2
    out = bytearray()

    for row in art:
        for plane in range(DEPTH):
            bits = bytearray(stride)
            for x, ch in enumerate(row):
                if (INDEX[ch] >> plane) & 1:
                    bits[x // 8] |= 0x80 >> (x % 8)
            out += bits
    return width, height, bytes(out)


def main():
    print("/* Erzeugt von icons.py - nicht von Hand aendern. */")
    print("")
    print('#include "icons.h"')
    print("")
    print("/* R, G, B je als 32 Bit, wie MUIA_Bitmap_SourceColors es erwartet. */")
    print("const ULONG icon_colors[%d] = {" % (len(PALETTE) * 3))
    for r, g, b in PALETTE:
        print("    0x%08lXUL, 0x%08lXUL, 0x%08lXUL," %
              (r * 0x01010101, g * 0x01010101, b * 0x01010101))
    print("};")
    print("")

    for name, art in IMAGES:
        width, height, body = planar(art)
        print("/* %s: %d x %d, %d Bitplanes */" % (name, width, height, DEPTH))
        print("const UBYTE icon_%s_body[%d] = {" % (name, len(body)))
        for i in range(0, len(body), 12):
            chunk = body[i:i + 12]
            print("    " + " ".join("0x%02X," % b for b in chunk))
        print("};")
        print("const struct IconDef icon_%s = { icon_%s_body, %d, %d, %d };"
              % (name, name, width, height, DEPTH))
        print("")


if __name__ == "__main__":
    main()
