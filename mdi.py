#!/usr/bin/env python3
"""Macht aus Material-Design-Icons Amiga-Bilddaten.

Home Assistant benutzt den MDI-Satz, und der ist unter Apache 2.0 frei
verwendbar. Die SVGs sind einfarbige Silhouetten mit Alphakanal - genau das,
was man braucht, um sie auf dem Amiga in einer festen Palette einzufaerben.

Der Weg: SVG von GitHub holen, mit dem macOS-eigenen qlmanage gross rendern,
mit Pillow auf 24x24 verkleinern (das gibt die weichen Kanten), und die
Deckung dann auf drei Stufen einer Farbrampe abbilden. Farbe 0 bleibt
durchsichtig.

    python3 mdi.py preview      Vorschau als PNG, stark vergroessert
    python3 mdi.py c > icons_mdi.c
"""

import os
import subprocess
import sys

from PIL import Image

SVG_URL = "https://raw.githubusercontent.com/Templarian/MaterialDesign-SVG/master/svg/%s.svg"
CACHE = "/tmp/mdi"
SIZE = 24
RENDER = 96            # gross rendern, dann verkleinern - das gibt die Kanten

# Eine gemeinsame Palette fuer alle Symbole. 16 Farben, Nummer 0 durchsichtig.
# Je Rampe drei Stufen: dunkel, mittel, hell.
PALETTE = [
    (0x00, 0x00, 0x00),   # 0  durchsichtig
    (0x30, 0x30, 0x30),   # 1  grau dunkel
    (0x70, 0x70, 0x70),   # 2  grau
    (0xB8, 0xB8, 0xB8),   # 3  grau hell
    (0x8A, 0x5A, 0x00),   # 4  bernstein dunkel
    (0xD0, 0x90, 0x00),   # 5  bernstein
    (0xFF, 0xC8, 0x30),   # 6  bernstein hell
    (0x1C, 0x50, 0x28),   # 7  gruen dunkel
    (0x22, 0xAA, 0x44),   # 8  gruen
    (0x70, 0xDD, 0x80),   # 9  gruen hell
    (0x1C, 0x3C, 0x60),   # 10 blau dunkel
    (0x30, 0x70, 0xB0),   # 11 blau
    (0x78, 0xB8, 0xE8),   # 12 blau hell
    (0x70, 0x20, 0x20),   # 13 rot dunkel
    (0xC0, 0x38, 0x38),   # 14 rot
    (0xF0, 0x80, 0x80),   # 15 rot hell
]

RAMPS = {
    "grau":       (1, 2, 3),
    "bernstein":  (4, 5, 6),
    "gruen":      (7, 8, 9),
    "blau":       (10, 11, 12),
    "rot":        (13, 14, 15),
}


def fetch(name):
    """Holt das SVG mit curl. Pythons urllib scheitert hier an den
    Wurzelzertifikaten, curl bringt seine eigenen mit."""
    os.makedirs(CACHE, exist_ok=True)
    svg = os.path.join(CACHE, name + ".svg")
    if not os.path.exists(svg) or os.path.getsize(svg) == 0:
        subprocess.run(["curl", "-sS", "--max-time", "30",
                        "-o", svg, SVG_URL % name], check=True)
        with open(svg, "rb") as f:
            if b"<path" not in f.read():
                os.remove(svg)
                raise SystemExit("kein Symbol namens %r bei MDI" % name)
    return svg


def render(name):
    """SVG -> Deckungsmaske 24x24, Werte 0..255.

    qlmanage rendert nicht auf durchsichtigem Grund, sondern schwarz auf
    weiss und voll deckend - der Alphakanal ist ueberall 255 und damit
    nutzlos. Die Deckung steckt in der Helligkeit, also umgekehrt: dunkel
    ist Symbol."""
    svg = fetch(name)
    png = os.path.join(CACHE, os.path.basename(svg) + ".png")
    if not os.path.exists(png):
        subprocess.run(["qlmanage", "-t", "-s", str(RENDER), "-o", CACHE, svg],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=False)
    if not os.path.exists(png):
        raise SystemExit("qlmanage konnte %s nicht rendern" % name)

    im = Image.open(png).convert("L")
    im = im.resize((SIZE, SIZE), Image.LANCZOS)
    return im.point(lambda v: 255 - v)


def to_indices(mask, ramp):
    """Deckung auf drei Stufen der Rampe abbilden."""
    dark, mid, light = RAMPS[ramp]
    out = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            a = mask.getpixel((x, y))
            if a < 32:
                row.append(0)
            elif a < 110:
                row.append(dark)
            elif a < 200:
                row.append(mid)
            else:
                row.append(light)
        out.append(row)
    return out


def planar(grid, depth=4):
    """ILBM-BODY: je Zeile erst Plane 0, dann 1 ... jeweils wortweise."""
    stride = ((SIZE + 15) // 16) * 2
    out = bytearray()
    for row in grid:
        for plane in range(depth):
            bits = bytearray(stride)
            for x, idx in enumerate(row):
                if (idx >> plane) & 1:
                    bits[x // 8] |= 0x80 >> (x % 8)
            out += bits
    return bytes(out)


def preview(icons, path="/tmp/mdi_preview.png", scale=6):
    cols = len(icons)
    img = Image.new("RGB", (cols * SIZE * scale, SIZE * scale), (0x95, 0x95, 0x95))
    px = img.load()
    for n, (name, ramp) in enumerate(icons):
        grid = to_indices(render(name), ramp)
        for y in range(SIZE):
            for x in range(SIZE):
                idx = grid[y][x]
                if idx == 0:
                    continue
                c = PALETTE[idx]
                for dy in range(scale):
                    for dx in range(scale):
                        px[(n * SIZE + x) * scale + dx, y * scale + dy] = c
    img.save(path)
    return path


def emit_c(icons):
    print("/* Erzeugt von mdi.py - nicht von Hand aendern.")
    print(" * Symbole: Material Design Icons, Apache 2.0,")
    print(" * https://github.com/Templarian/MaterialDesign */")
    print("")
    print('#include "icons.h"')
    print("")
    print("const ULONG mdi_colors[%d] = {" % (len(PALETTE) * 3))
    for r, g, b in PALETTE:
        print("    0x%08lXUL, 0x%08lXUL, 0x%08lXUL," %
              (r * 0x01010101, g * 0x01010101, b * 0x01010101))
    print("};")
    print("")
    for name, ramp in icons:
        body = planar(to_indices(render(name), ramp))
        cname = name.replace("-", "_")
        print("/* %s (%s) */" % (name, ramp))
        print("const UBYTE mdi_%s_body[%d] = {" % (cname, len(body)))
        for i in range(0, len(body), 12):
            print("    " + " ".join("0x%02X," % b for b in body[i:i + 12]))
        print("};")
        print("const struct IconDef mdi_%s = { mdi_%s_body, %d, %d, 4 };"
              % (cname, cname, SIZE, SIZE))
        print("")

    print("static const struct IconDef *MDI_TABLE[MDI_COUNT] = {")
    for name, _ in icons:
        print("    &mdi_%s," % name.replace("-", "_"))
    print("};")
    print("")
    print("const struct IconDef *mdi_icon(int n)")
    print("{")
    print("    if (n < 0 || n >= MDI_COUNT) {")
    print("        n = 13;                 /* Rueckfall: das Haus */")
    print("    }")
    print("    return MDI_TABLE[n];")
    print("}")


# Die Symbole. Die Reihenfolge ist die Bildnummer - also nur hinten
# anhaengen, sonst zeigen gespeicherte Dashboards auf das falsche Bild.
# 0..11 sind die Raeume, auf die dash.c/icon_for_area() abbildet,
# 12 ist "Ohne Raum", 13 der Rueckfall. Danach kommt Freies.
ICONS = [
    ("desk",                  "blau"),      #  0 Buero
    ("shower",                "blau"),      #  1 Bad
    ("home-roof",             "blau"),      #  2 Dachboden
    ("garage",                "blau"),      #  3 Garage
    ("flower",                "gruen"),     #  4 Garten
    ("stairs-down",           "blau"),      #  5 Keller
    ("countertop",            "blau"),      #  6 Kueche
    ("bed",                   "blau"),      #  7 Schlafzimmer
    ("toilet",                "blau"),      #  8 Toilette
    ("stairs",                "blau"),      #  9 Treppe
    ("washing-machine",       "blau"),      # 10 Waschkueche
    ("sofa",                  "blau"),      # 11 Wohnzimmer
    ("dots-horizontal",       "grau"),      # 12 Ohne Raum
    ("home",                  "blau"),      # 13 Haus
    ("view-dashboard",        "blau"),      # 14 Uebersicht
    ("lightning-bolt",        "bernstein"), # 15 Energie
    ("solar-power-variant",   "bernstein"), # 16 Solar
    ("battery",               "gruen"),     # 17 Batterie
    ("thermometer",           "rot"),       # 18 Temperatur
    ("water-percent",         "blau"),      # 19 Feuchte
    ("weather-partly-cloudy", "blau"),      # 20 Wetter
    ("lightbulb",             "bernstein"), # 21 Licht
    ("power-socket-de",       "grau"),      # 22 Steckdose
    ("window-closed-variant", "grau"),      # 23 Fenster
    ("door",                  "grau"),      # 24 Tuer
    ("lock",                  "gruen"),     # 25 Schloss
    ("shield-home",           "gruen"),     # 26 Sicherheit
    ("car",                   "blau"),      # 27 Auto
    ("television",            "blau"),      # 28 Fernsehen
    ("music",                 "blau"),      # 29 Musik
    ("wifi",                  "blau"),      # 30 Netzwerk
    ("blinds",                "blau"),      # 31 Rollladen
    ("fan",                   "blau"),      # 32 Luefter
    ("clock-outline",         "grau"),      # 33 Zeit
    ("tools",                 "grau"),      # 34 Werkzeug
    ("printer",               "grau"),      # 35 Drucker
]

if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "preview"
    if what == "c":
        emit_c(ICONS)
    else:
        print(preview(ICONS))
