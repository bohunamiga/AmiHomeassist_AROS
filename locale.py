#!/usr/bin/env python3
"""Erzeugt AmigaOS-Kataloge (.catalog) und den C-Header aus strings.cd/*.ct.

Ersatz fuer catcomp/FlexCat: FlexCat laesst sich nicht cross bauen (es ist
auf Amiga-Header verdrahtet), und die Toolchain bringt kein catcomp mit.
Das Format ist aus FlexCats createcat.c gelesen und gegen einen echten
Katalog von der Amiga (LOCALE:Catalogs/deutsch/screentime.catalog)
byteweise gegengeprueft - siehe selftest().

    python3 locale.py            # baut locale_strings.h + catalogs/
    python3 locale.py --selftest # prueft den Schreiber gegen echte Kataloge

Dateiformat:

    FORM <len> CTLG
      FVER <len> "$VER: name.catalog v.r (d.m.yyyy)\\0"   auf gerade gepolstert
      LANG <len> "deutsch\\0"                             auf gerade gepolstert
      CSET <len> 32 Nullbytes
      STRS <len> je String: LONG id, LONG len, Bytes, Polster auf Vielfaches von 4

Die Laenge eines STRS-Eintrags ist die reine Stringlaenge PLUS EINS, wenn
diese durch 4 teilbar ist - so folgt immer mindestens ein Nullbyte. Genau
das macht FlexCats CatPuts(), und ohne diese Regel liest locale.library
ueber das Stringende hinaus.
"""

import os
import struct
import sys


def _chunk(cid, body, pad_to=2):
    """Ein IFF-Chunk: ID, Laenge, Inhalt, auf pad_to gepolstert."""
    out = cid + struct.pack(">I", len(body)) + body
    if len(out) % pad_to:
        out += b"\0" * (pad_to - len(out) % pad_to)
    return out


def _strs(strings, nul_in_len=True):
    """Der STRS-Chunk-Inhalt aus [(id, bytes), ...].

    Es sind zwei Konventionen im Umlauf, beide laufen auf echten Amigas,
    weil beide auf 4 Byte polstern und damit dieselbe Grenze treffen:

      nul_in_len=True   catcomp-Stil: deklariert ist immer Laenge+1, das
                        Nullbyte gehoert also zur angegebenen Laenge.
                        (so gebaut: LOCALE:Catalogs/deutsch/GiggleDisk.catalog)
      nul_in_len=False  FlexCat-Stil: deklariert ist die reine Laenge, +1
                        nur wenn sie durch 4 teilbar ist - damit auch dann
                        ein Nullbyte folgt.
                        (so gebaut: .../deutsch/screentime.catalog)

    Vorgabe ist der catcomp-Stil: dort schliesst die angegebene Laenge das
    Nullbyte immer ein, was fuer jeden Leser die sicherere Zusage ist.
    """
    body = b""
    for sid, text in strings:
        if nul_in_len:
            declared = len(text) + 1
        else:
            declared = len(text) + (1 if len(text) % 4 == 0 else 0)
        pad = (-declared) % 4
        body += struct.pack(">II", sid, declared) + text + b"\0" * (
            declared - len(text) + pad)
    return body


def build_catalog(language, version_line, strings, nul_in_len=True, codeset=0):
    """Baut eine vollstaendige .catalog-Datei als bytes."""
    body = b"CTLG"
    body += _chunk(b"FVER", to_amiga(version_line) + b"\0")
    body += _chunk(b"LANG", to_amiga(language) + b"\0")
    body += _chunk(b"CSET", struct.pack(">I", codeset) + b"\0" * 28)
    body += _chunk(b"STRS", _strs(strings, nul_in_len))
    return b"FORM" + struct.pack(">I", len(body)) + body


def parse_catalog(data):
    """Liest eine .catalog-Datei zurueck - fuer den Selbsttest."""
    assert data[:4] == b"FORM" and data[8:12] == b"CTLG", "kein CTLG-IFF"
    out = {"strings": []}
    off = 12
    while off < len(data):
        cid = data[off:off + 4]
        ln = struct.unpack(">I", data[off + 4:off + 8])[0]
        chunk = data[off + 8:off + 8 + ln]
        if cid == b"STRS":
            o = 0
            while o < ln:
                sid, slen = struct.unpack(">II", chunk[o:o + 8])
                o += 8
                out["strings"].append((sid, chunk[o:o + slen].rstrip(b"\0")))
                o += slen + (-slen) % 4
        elif cid == b"CSET":
            out["cset"] = struct.unpack(">I", chunk[:4])[0]
        else:
            out[cid.decode()] = chunk.rstrip(b"\0")
        off += 8 + ln + (ln & 1)
    return out


def selftest(paths):
    """Liest echte Kataloge und schreibt sie neu - Bytes muessen gleich sein."""
    ok = True
    for p in paths:
        original = open(p, "rb").read()
        c = parse_catalog(original)
        # Beide Konventionen durchprobieren - der Katalog muss mit einer
        # von beiden byteweise identisch herauskommen.
        same = False
        for style in (True, False):
            rebuilt = build_catalog(c["LANG"].decode("latin-1"),
                                    c["FVER"].decode("latin-1"),
                                    c["strings"], nul_in_len=style,
                                    codeset=c.get("cset", 0))
            if rebuilt == original:
                same = True
                used = "catcomp" if style else "FlexCat"
                break
        ok = ok and same
        print("%-28s %4d Bytes, %2d Strings  %s"
              % (os.path.basename(p), len(original), len(c["strings"]),
                 "identisch (%s-Stil)" % used if same else "ABWEICHUNG"))
        if not same:
            for i in range(min(len(original), len(rebuilt))):
                if original[i] != rebuilt[i]:
                    print("   erste Abweichung bei Byte %d: %02x != %02x"
                          % (i, original[i], rebuilt[i]))
                    break
            print("   Laengen: original %d, neu %d" % (len(original), len(rebuilt)))
    return ok


# --------------------------------------------------------------------------
# strings.cd / *.ct einlesen und daraus Header + Kataloge erzeugen
# --------------------------------------------------------------------------

VERSION = "0.7"
DATE    = "01.09.2026"
CATALOG_NAME = "AmiHomeassist.catalog"


def to_amiga(s):
    """Text nach Latin-1, dem Amiga-Zeichensatz.

    Alles, was die vier Sprachen brauchen - ä ö ü ß, à è ì ò ù, á é í ó ú ñ,
    ¿ ¡ - liegt in Latin-1. Was sich nicht abbilden laesst, faellt hart auf,
    statt still zu Muell zu werden.
    """
    try:
        return s.encode("latin-1")
    except UnicodeEncodeError as e:
        raise SystemExit("Zeichen nicht in Latin-1 darstellbar: %r in %r"
                         % (s[e.start:e.end], s))


# ISO-8859-2 (Mittel-/Osteuropa) fuer Sprachen wie 'polski'. Die Katalog-
# Dateien sind wie alle anderen UTF-8; erst beim Schreiben wird umgesetzt.
# Der zugehoerige Codeset-Wert im CSET-Chunk ist 5 (siehe AROS workbench).
LATIN2_MAP = {
    "\u0104": b"\xa1",   # Ą
    "\u0106": b"\xc6",   # Ć
    "\u0118": b"\xca",   # Ę
    "\u0141": b"\xa3",   # Ł
    "\u0143": b"\xd1",   # Ń
    "\u00d3": b"\xd3",   # Ó
    "\u015a": b"\xa6",   # Ś
    "\u0179": b"\xac",   # Ź
    "\u017b": b"\xaf",   # Ż
    "\u0105": b"\xb1",   # ą
    "\u0107": b"\xe6",   # ć
    "\u0119": b"\xea",   # ę
    "\u0142": b"\xb3",   # ł
    "\u0144": b"\xf1",   # ń
    "\u00f3": b"\xf3",   # ó
    "\u015b": b"\xb6",   # ś
    "\u017a": b"\xbc",   # ź
    "\u017c": b"\xbf",   # ż
}


def to_latin2(s):
    """UTF-8 nach ISO-8859-2 umsetzen - fuer polski. Hart abbrechen bei
    Zeichen, die es dort nicht gibt (statt still zu Muell zu werden)."""
    out = bytearray()
    for ch in s:
        if ord(ch) < 128:
            out.append(ord(ch))
        elif ch in LATIN2_MAP:
            out += LATIN2_MAP[ch]
        else:
            raise SystemExit(
                "Zeichen nicht in ISO-8859-2 darstellbar: %r in %r" % (ch, s))
    return bytes(out)


def codec_for(codeset):
    """Der Zeichensatz-Uebersetzer zur Codeset-Nummer aus ## codeset."""
    return to_latin2 if codeset == 5 else to_amiga


def unescape(s, enc=to_amiga):
    """C-Escapes in echte Bytes wandeln - fuer die Katalogdatei.

    Gebraucht werden \n, \t, \\ und oktale Folgen wie \33 (ESC), mit dem
    MUI seine Textauszeichnung einleitet. 'enc' ist der Zeichensatz-
    Uebersetzer fuer den Codeset der Sprache.
    """
    out = bytearray()
    i = 0
    while i < len(s):
        if s[i] != "\\":
            out += enc(s[i])
            i += 1
            continue
        i += 1
        c = s[i]
        if c in "01234567":
            j = i
            while j < len(s) and j < i + 3 and s[j] in "01234567":
                j += 1
            out.append(int(s[i:j], 8))
            i = j
        else:
            out += {"n": b"\n", "t": b"\t", "\\": b"\\",
                    '"': b'"', "e": b"\x1b"}.get(c, enc(c))
            i += 1
    return bytes(out)


def read_cd(path):
    """strings.cd lesen -> [(name, id, roher Text), ...] in Reihenfolge.

    Die Datei ist UTF-8 - so tippt man sie auf einem heutigen Rechner. Der
    Amiga braucht Latin-1; umgewandelt wird erst beim Schreiben, siehe
    to_amiga().
    """
    entries = []
    next_id = 0
    pending = None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if pending is not None:
            entries.append((pending[0], pending[1], line))
            pending = None
            continue
        if not line or line[0] in ";#":
            continue
        if "(" in line and line.rstrip().endswith(")"):
            name = line.split("(")[0].strip()
            spec = line[line.index("(") + 1:line.rindex(")")]
            given = spec.split("/")[0].strip()
            sid = int(given) if given else next_id
            next_id = sid + 1
            pending = (name, sid)
    return entries


def read_ct(path):
    """Eine Uebersetzung lesen -> (sprache, codeset, {name: roher Text})."""
    lang = None
    codeset = 0
    out = {}
    pending = None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("## language"):
            lang = line.split(None, 2)[2].strip()
            continue
        if line.startswith("## codeset"):
            codeset = int(line.split(None, 2)[2].strip() or 0)
            continue
        # Die Pruefung auf einen wartenden Namen MUSS vor der auf
        # Kommentare stehen: mehrere Texte fangen selbst mit ';' an - es
        # sind die Kommentarzeilen, die das Programm in seine
        # Konfigurationsdateien schreibt. Andersherum verschluckt der
        # Leser sie und schiebt alle folgenden Werte um einen Eintrag
        # weiter, ohne sich zu beschweren.
        if pending is not None:
            if line.strip():
                out[pending] = line
            pending = None
            continue
        if line.startswith("#") or (line[:1] == ";"):
            continue
        if not line.strip():
            continue
        pending = line.strip()
    return lang, codeset, out


def write_header(entries, path):
    """locale_strings.h: die MSG_-Nummern und die eingebauten Texte."""
    with open(path, "w", encoding="utf-8") as f:
        f.write("/* Von locale.py aus strings.cd erzeugt. NICHT von Hand aendern. */\n")
        f.write("#ifndef LOCALE_STRINGS_H\n#define LOCALE_STRINGS_H\n\n")
        for name, sid, _ in entries:
            f.write("#define %-24s %d\n" % (name, sid))
        f.write("\n#define AMILOC_COUNT %d\n\n" % len(entries))
        f.write("/* Die eingebauten Texte sind Englisch - siehe amiloc.h. */\n")
        f.write("static const char *const AMILOC_BUILTIN[AMILOC_COUNT] = {\n")
        for name, sid, text in entries:
            f.write('    /* %-3d */ "%s",\n' % (sid, text))
        f.write("};\n\n#endif\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    entries = read_cd(os.path.join(here, "strings.cd"))
    write_header(entries, os.path.join(here, "locale_strings.h"))
    print("locale_strings.h: %d Strings" % len(entries))

    by_name = {n: (i, t) for n, i, t in entries}
    ctdir = os.path.join(here, "catalogs")
    outdir = os.path.join(here, "dist-catalogs")
    if not os.path.isdir(ctdir):
        print("kein catalogs/ - nur der Header wurde gebaut")
        return
    for ct in sorted(os.listdir(ctdir)):
        if not ct.endswith(".ct"):
            continue
        lang, codeset, trans = read_ct(os.path.join(ctdir, ct))
        enc = codec_for(codeset)
        strings = []
        missing = []
        for name, sid, builtin in entries:
            if name in trans:
                value = trans[name]
                # Sieht ein Wert aus wie ein MSG_-Name, ist die Datei
                # verrutscht - lieber hart abbrechen als stillschweigend
                # falsche Texte ausliefern.
                if value.strip() in by_name:
                    raise SystemExit(
                        "%s: %s hat als Text den Namen %s - die Datei ist "
                        "um einen Eintrag verrutscht"
                        % (ct, name, value.strip()))
                strings.append((sid, unescape(value, enc)))
            else:
                missing.append(name)
        ver = "$VER: %s %s (%s)" % (CATALOG_NAME, VERSION, DATE)
        data = build_catalog(lang, ver, strings, codeset=codeset)
        d = os.path.join(outdir, lang)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, CATALOG_NAME), "wb").write(data)
        note = "" if not missing else "  (%d ohne Uebersetzung: %s)" % (
            len(missing), ", ".join(missing[:3]) + ("..." if len(missing) > 3 else ""))
        print("%-10s %4d Bytes, %2d Strings%s" % (lang, len(data), len(strings), note))


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        files = [a for a in sys.argv[1:] if not a.startswith("--")]
        sys.exit(0 if selftest(files) else 1)
    main()
