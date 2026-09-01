# AmiHomeassist — Cross-Build mit m68k-amigaos-gcc (AmigaPorts-Toolchain).
#
# Zielprofil: 68020 aufwaerts, MIT UND OHNE FPU. Deshalb -mcpu=68020 und
# sonst nichts — kein -m68030/040/060, kein -m68881. Das waehlt die
# libm020-Multilib (Softfloat) und erzeugt ein Binary, das von einem
# blanken A1200 bis zum 060 auf jeder Maschine laeuft.
#
#   make            → AmiHomeassist + AmiHomeassistCLI
#   make check-fpu  → prueft, dass keine FPU-Instruktion im Binary steckt
#
# Loest das alte mk-Skript ab (SAS/C 6.59 auf der Amiga). Alle dortigen
# Workarounds entfallen ersatzlos: kein DATA=FARONLY (gcc adressiert
# ohnehin 32-bittig), kein scnb.lib-vs-sc.lib, kein IGNORE=84.

TOOLCHAIN ?= $(HOME)/opt/m68k-amigaos-gcc-16.2
CC         = $(TOOLCHAIN)/bin/m68k-amigaos-gcc
OBJDUMP    = $(TOOLCHAIN)/bin/m68k-amigaos-objdump

CPU        = 68020

# -MMD -MP schreibt je Objekt eine .d-Datei mit den Headern, die es
# einbindet. Ohne das uebersetzt make nach einer Header-Aenderung nichts
# neu und man testet stillschweigend das alte Binary weiter.
CFLAGS  = -mcpu=$(CPU) -Os -fomit-frame-pointer -noixemul -MMD -MP \
          -Wall -Wextra -Wno-unused-parameter -Wno-pointer-sign \
          -Ivendor/mui/include -I.
# -noixemul waehlt libnix statt newlib.
#
# ACHTUNG: hier steht bewusst KEIN -lamiga. amiga.lib bringt ein eigenes
# _sprintf mit, die RawDoFmt-Fassung, und dort ist %d SECHZEHN Bit breit.
# Mit -lamiga in der Linkzeile gewann diese Fassung gegen die von libnix:
# sprintf("%d", 400) schrieb "0" (die obere Haelfte des Langwortes) und
# schob alle folgenden Argumente um zwei Bytes. Der HTTP-Request ging
# damit als "Host: homeassistant:0" mit Muell-Token raus, Home Assistant
# antwortete mit 400 Bad Request. Aus amiga.lib brauchen wir ohnehin nur
# DoMethod() - das holen wir uns unten als einzelnes Objekt heraus.
LDFLAGS = -noixemul -s -lgcc

AR       = $(TOOLCHAIN)/bin/m68k-amigaos-ar
AMIGALIB = $(TOOLCHAIN)/m68k-amigaos/lib/libamiga.a

# muistubs.c MUSS eine eigene Uebersetzungseinheit bleiben — siehe den
# Kommentar in der Datei. Als static __inline im Header wirft gcc bei -Os
# die variadischen Argumente weg, und jedes MUI-Objekt bekommt eine
# Muell-Tagliste.
GUI_OBJS = amiha.o dash.o edit.o gui.o icons.o icons_mdi.o logo.o muistubs.o \
           amiloc.o \
           build/DoMethod.o
CLI_OBJS = amiha.o dash.o cli.o amiloc.o

all: locale_strings.h AmiHomeassist AmiHomeassistCLI

# Kataloge und die MSG_-Nummern entstehen aus strings.cd und catalogs/*.ct.
# locale.py ersetzt catcomp/FlexCat - siehe den Kopf der Datei.
locale_strings.h: strings.cd $(wildcard catalogs/*.ct) locale.py
	python3 locale.py

AmiHomeassist: $(GUI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GUI_OBJS) $(LDFLAGS)

AmiHomeassistCLI: $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJS) $(LDFLAGS)

%.o: %.c locale_strings.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Nur DoMethod()/DoMethodA() aus amiga.lib, statt der ganzen Bibliothek -
# siehe die sprintf-Warnung oben bei LDFLAGS.
build/DoMethod.o: $(AMIGALIB)
	@mkdir -p build
	cd build && $(AR) x $(AMIGALIB) DoMethod.o

# Harte Zusicherung fuers Zielprofil: kein einziger FPU-Opcode.
#
# Geprueft wird ausschliesslich die MNEMONIK-Spalte, nicht die Zeile. Die
# erste Fassung griff auf die ganze objdump-Zeile zu und schlug bei
# "4eba fbcc  jsr %pc@(0x250)" an - die Bytefolge fbcc in der Hex-Spalte
# sah aus wie ein fbcc-Sprung. Auf dem 68k faengt jeder 68881/2-Befehl mit
# f an, die Spalte allein ist also das richtige und einfachere Kriterium.
check-fpu: AmiHomeassist AmiHomeassistCLI
	@for f in AmiHomeassist AmiHomeassistCLI; do \
	  n=$$($(OBJDUMP) -d $$f | awk -F'\t' 'NF>=3 { split($$3, m, " "); if (m[1] ~ /^f/) c++ } END { print c+0 }'); \
	  if [ "$$n" -ne 0 ]; then \
	    echo "FEHLER: $$f enthaelt $$n FPU-Instruktionen:"; \
	    $(OBJDUMP) -d $$f | awk -F'\t' 'NF>=3 { split($$3, m, " "); if (m[1] ~ /^f/) print "   " $$0 }' | head -5; \
	    exit 1; \
	  fi; \
	  echo "OK: $$f ist FPU-frei ($$(ls -l $$f | awk '{print $$5}') Bytes)"; \
	done

-include $(GUI_OBJS:.o=.d) $(CLI_OBJS:.o=.d)

clean:
	rm -rf *.o *.d build AmiHomeassist AmiHomeassistCLI
	rm -rf locale_strings.h dist-catalogs

.PHONY: all clean check-fpu
