.key FILE
; Uebersetzt AmiHomeassist mit SAS/C 6.59 auf dem Amiga.
;
; Gebaut wird fuer 68020 ohne FPU, damit es auch auf echten 68030 laeuft.
; DATA=FARONLY, weil die Geraeteliste als ein Objekt groesser als 64 KB ist.
;
; amiha.c braucht die AmiTCP-Header, die anderen nicht - deshalb wird nur
; dort IDIR=SASC:netinclude/ gesetzt. Das Verzeichnis bringt eigene Fassungen
; von proto/dos.h und stdio.h mit und wuerde sonst SAS/Cs eigene verdecken.
;
; net.lib wird bewusst NICHT gelinkt: zusammen mit dem eigenen
; OpenLibrary("bsdsocket.library") stuerzt das Programm beim Start ab.

assign >NIL: SASC: Stuff:Developer/SASC
assign >NIL: SC: SASC:
assign >NIL: lib: SC:lib
assign >NIL: include: SC:include
assign >NIL: C: SC:C ADD
stack 200000
cd RAM:AmiHomeassist

echo "amiha.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY IDIR=SASC:netinclude/ amiha.c
echo "cli.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY cli.c
echo "dash.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY dash.c
echo "edit.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY IDIR=RAM:AmiHomeassist/ IGNORE=84 edit.c
echo "icons.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY icons.c
; IDIR aufs Projektverzeichnis, weil NListview_mcc.h intern
; <mui/NList_mcc.h> in spitzen Klammern einbindet - das sucht nur im
; Include-Pfad, nicht im aktuellen Verzeichnis.
; IGNORE=84: SAS/C kuerzt Praeprozessornamen auf 32 Zeichen, wodurch die
; langen MUIV_NListtree_...-Namen scheinbar kollidieren. Harmlos.
echo "logo.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY logo.c
echo "icons_mdi.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY icons_mdi.c
echo "gui.c ..."
sc CPU=68020 NOSTACKCHECK NOVERSION DATA=FARONLY IDIR=RAM:AmiHomeassist/ IGNORE=84 gui.c

echo "linken ..."
; Kein SC/SD: die beiden Schalter zwingen Code und Daten in je eine
; 64-KB-Sektion mit 16-Bit-Relokation, und daran scheitert die Geraeteliste
; ("MERGED data > 64K", "Distance for Data Reloc16 > 32768").
;
; scnb.lib statt sc.lib! sc.lib ist die basisrelative Fassung und setzt ein
; gueltiges A4 voraus. Mit DATA=FARONLY richtet der Compiler A4 nicht mehr
; ein, und dann greift schon das erste malloc() ins Leere und haengt.
; Genau das kostete einen halben Vormittag; SAS/C selbst waehlt beim
; eingebauten LINK ebenfalls scnb.lib (nachzulesen in der erzeugten .lnk).
slink FROM LIB:c.o amiha.o dash.o cli.o TO AmiHomeassistCLI LIB LIB:scnb.lib LIB:amiga.lib NOICONS
slink FROM LIB:c.o amiha.o dash.o edit.o gui.o icons.o icons_mdi.o logo.o TO AmiHomeassist LIB LIB:scnb.lib LIB:amiga.lib NOICONS
echo "fertig."
