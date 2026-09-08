AmiHomeassist 0.7
=================

  English below, deutsche Fassung weiter unten.
  Deutsche Fassung ab "AmiHomeassist 0.7 - Deutsch".


ENGLISH
=======

Home Assistant from a real Amiga: lights, sockets, blinds and sensors
arranged on dashboards of your own, with a MUI interface.

A sidebar on the left with your dashboards, the contents of the chosen
page on the right in framed groups. One click on the switch flips the
device. States are polled, so changes made elsewhere show up by
themselves.


WHAT YOU NEED

  AmigaOS 2.0 or newer, 68020 or better. No FPU needed.

  MUI 3.8 or newer (muimaster.library).

  The NList classes:  NList.mcc, NListview.mcc
  This is the only thing beyond a plain MUI installation - MUI does NOT
  ship these. Get them from Aminet as dev/mui/MCC_NList.lha, or newer
  from https://github.com/amiga-mui/nlist
  The .mcc files belong in MUI:Libs/MUI.
  Check with:  Version MUI:Libs/MUI/NList.mcc

  A TCP/IP stack offering bsdsocket.library - Roadshow, AmiTCP, Miami
  and the rest all work.

  Home Assistant reachable over plain *http*. https is not supported
  and is refused with a clear message.

  A long-lived access token from Home Assistant: your profile, Security
  tab, at the bottom under "Long-lived access tokens".


SETTING UP

  1. Copy the drawer wherever you like, for instance to SYS:Tools.

  2. Start AmiHomeassist. On the first run the settings window opens by
     itself. Enter the address and the token, then Save.

         Address      http://homeassistant:8123
         Interval     how often states are fetched

     Over a slow link use 15 or 30 seconds.

  3. Then the device picker opens with a suggestion. That suggestion is
     only a guess - every installation looks different. Look it over,
     adjust, then Apply. One page per room is created from it.

  4. Want it different? Use "Edit ...".


THE THREE WINDOWS

  Device list   the main window with the sidebar

  Select        which devices the program knows at all. "Suggest"
                hides the usual ballast: the per-device
                internet-access switches of a Fritzbox, the Dnd lamps
                of smart sockets, technical helper switches and
                anything unavailable.

  Edit          create pages, form groups, add devices, choose the
                widget kind and the icon, reorder.


WIDGET KINDS

  Switch      light and switch - the toggle you click
  Indicator   binary_sensor - window open or closed
  Number      sensor with its unit
  Bar         sensor with a fixed range, per cent or watts
  Blind       cover - up, stop, down, and the position
  Thermostat  climate - room and target temperature, warmer, colder,
              and the operating mode
  Text        a caption

  When you add a device the program suggests a kind, from its domain
  and device class. The editor lets you change it.


LANGUAGES

  The interface speaks English, German, Italian and Spanish. English is
  built into the program; the other three sit in the Catalogs drawer and
  are loaded through locale.library. If that library is missing, or no
  catalogue matches, everything simply carries on in English.

  Normally the language follows your system setting in Prefs/Locale, so
  there is nothing to do. If you run an English Workbench but want this
  program in your own language, set a variable:

      SetEnv SAVE AmiHomeassistLanguage deutsch

  Valid values are the drawer names: deutsch, italiano, espanol (with
  the tilde). Back to English:

      UnSetEnv AmiHomeassistLanguage
      Delete ENVARC:AmiHomeassistLanguage

  Both lines are needed - SAVE put a copy in ENVARC:.

  Device names, room names, units and states come from Home Assistant
  and are therefore already in your own language.

  Group headings on a dashboard are written into Dashboards.prefs when
  a page is created, so they keep the language they were made in.
  Changing the language does not rewrite them; new pages come out in
  the new language, and the editor renames the old ones.

  Italian and Spanish were not written by native speakers. The .ct
  files in the Source drawer are plain text - corrections are welcome
  and need no recompilation.


FILES

  ENVARC:AmiHomeassist/AmiHomeassist.prefs   address, token, interval
  ENVARC:AmiHomeassist/Import.prefs          the devices taken over
  ENVARC:AmiHomeassist/Dashboards.prefs      the pages

  All three are plain text and can be repaired by hand if need be.

  The token is stored in the clear and grants full access to your Home
  Assistant. On your own network that is acceptable, but you should
  know it.


FROM THE SHELL

  AmiHomeassistCLI                 list by room, filtered
  AmiHomeassistCLI ALL             including the hidden entries
  AmiHomeassistCLI DOMAIN=sensor   one domain only
  AmiHomeassistCLI STATES          shows the frugal mode
  AmiHomeassistCLI ON=light.kueche
  AmiHomeassistCLI OFF=switch.beamer
  AmiHomeassistCLI TOGGLE=light.tisch

  Setting up works without the interface too:

  AmiHomeassistCLI HOST=http://homeassistant:8123 TOKEN=<token> SAVE


HOW THE ROOMS GET IN

  The REST interface of Home Assistant does not report areas - they
  live in the registry behind the WebSocket interface. Rather than
  rebuild that, AmiHomeassist sends a Jinja template to /api/template
  and lets Home Assistant resolve the mapping itself. What comes back
  are finished lines. So the Amiga needs neither WebSocket nor a JSON
  parser.


CREDITS AND RIGHTS

  Icons are Material Design Icons, Apache 2.0:
  https://github.com/Templarian/MaterialDesign

  The house logo belongs to the Home Assistant project. Home Assistant
  is a separate project and unaffiliated with this program:
  https://www.home-assistant.io/

  Source included, MIT licence. See the Source drawer.

  muistubs.c - the out-of-line varargs stubs for muimaster.library -
  comes from the amimcp project and is Apache 2.0, not MIT. The licence
  text is in Source/LICENSE-Apache-2.0, the origin is named in the file
  itself: https://github.com/thomas-luebker/amimcp

  This program is vibe coded. I described what it should do, an AI
  wrote the code, and I read, tested and decided what stayed in. The
  full source is here so you can judge for yourself.

BUILDING IT YOURSELF

  Cross-compiled with m68k-amigaos-gcc (AmigaPorts), not with SAS/C on
  the Amiga as 0.5 was. In the Source drawer:

      make

  Target profile is 68020 and up, with and without FPU, so the flags
  are -mcpu=68020 and nothing else. "make check-fpu" proves by
  disassembly that not one FPU instruction made it into the binary.

  Two things are NOT in this archive and you must supply them:

    MUI's own developer headers (libraries/mui.h, proto/muimaster.h,
    clib/muimaster_protos.h, inline/muimaster.h). MUI's licence allows
    redistribution of the complete original archive only, so they
    cannot ride along here. Take them from the MUI developer archive
    and put them under Source/vendor/mui/include/.

    An m68k-amigaos cross toolchain:
    https://github.com/AmigaPorts/m68k-amigaos-gcc

  The NList class headers ARE included, under Source/mui/ - they are
  LGPL. They are only headers; the .mcc classes themselves you install
  from Aminet as described above.

  locale.py turns strings.cd and catalogs/*.ct into the catalogues and
  into locale_strings.h. That header is included ready-made, so a build
  works without Python.

  Tested on an A500 with PiStorm (Emu68), OS 3.2.3, MUI 3.8. A real
  68030 is still to come.


================================================================

AmiHomeassist 0.7 - Deutsch
===========================

Home Assistant vom Amiga aus: Lampen, Steckdosen, Rollaeden und
Sensoren nach eigenen Dashboards geordnet, mit MUI-Oberflaeche.

Links eine Seitenleiste mit deinen Dashboards, rechts deren Inhalt in
umrandeten Gruppen. Ein Klick auf den Schalter legt das Geraet um. Der
Zustand wird zyklisch nachgefuehrt, Aenderungen von aussen erscheinen
also von selbst.


WAS DU BRAUCHST
---------------

  AmigaOS 2.0 oder neuer, 68020 oder besser. Keine FPU noetig.

  MUI 3.8 oder neuer (muimaster.library).

  Die NList-Klassen:  NList.mcc, NListview.mcc
  Das ist das Einzige, was ueber eine gewoehnliche MUI-Installation
  hinausgeht - MUI bringt diese Klassen NICHT mit. Zu finden auf Aminet
  als dev/mui/MCC_NList.lha, oder aktueller unter
  https://github.com/amiga-mui/nlist
  Die .mcc-Dateien gehoeren nach MUI:Libs/MUI.
  Pruefen mit:  Version MUI:Libs/MUI/NList.mcc

  Ein TCP/IP-Stack, der bsdsocket.library bereitstellt - Roadshow,
  AmiTCP, Miami und alles andere Uebliche tun es.

  Home Assistant, ueber *http* erreichbar. https wird nicht
  unterstuetzt und mit einer klaren Meldung abgelehnt.

  Einen Long-Lived Access Token aus Home Assistant: Profil, Reiter
  Sicherheit, ganz unten "Langlebige Zugriffstoken".


EINRICHTEN
----------

  1. Das Verzeichnis irgendwohin kopieren, zum Beispiel nach SYS:Tools.

  2. AmiHomeassist starten. Beim ersten Mal geht das Einstellungsfenster
     von selbst auf. Adresse und Token eintragen, Speichern.

         Adresse      http://homeassistant:8123
         Abstand (s)  wie oft der Zustand geholt wird

     Ueber eine langsame Leitung ruhig 15 oder 30 Sekunden nehmen.

  3. Danach oeffnet sich die Geraeteauswahl mit einem Vorschlag. Der ist
     nur geraten - jede Installation sieht anders aus. Durchsehen,
     anpassen, Uebernehmen. Daraus entsteht je Raum eine Seite.

  4. Wer es anders will, nimmt "Bearbeiten ...".


DIE DREI FENSTER
----------------

  Geraeteliste   das Hauptfenster mit der Seitenleiste

  Auswahl        welche Geraete das Programm ueberhaupt kennt.
                 "Vorschlag" blendet den ueblichen Ballast aus: die
                 Internet-access-Schalter einer Fritzbox, die
                 Dnd-Laempchen von Steckdosen, technische
                 Nebenschalter und alles Unerreichbare.

  Bearbeiten     Seiten anlegen, Gruppen bilden, Geraete hineinnehmen,
                 Darstellung und Symbol waehlen, umsortieren.


DARSTELLUNGSARTEN
-----------------

  Schalter    light und switch - der Schalter zum Klicken
  Laempchen   binary_sensor - Fenster offen oder zu
  Zahl        sensor mit Einheit
  Balken      sensor mit festem Bereich, etwa Prozent oder Watt
  Rollladen   cover - Auf, Stop, Zu und die Stellung
  Thermostat  climate - Ist- und Solltemperatur, waermer, kaelter und
              die Betriebsart
  Text        eine Zwischenueberschrift

  Beim Hinzufuegen schlaegt das Programm eine Art vor, aus Domain und
  Geraeteklasse. Im Editor laesst sie sich aendern.


SPRACHEN
--------

  Die Oberflaeche spricht Englisch, Deutsch, Italienisch und Spanisch.
  Englisch steckt fest im Programm; die anderen drei liegen im
  Verzeichnis Catalogs und werden ueber locale.library geladen. Fehlt
  die Bibliothek oder passt kein Katalog, laeuft alles auf Englisch
  weiter - ohne Meldung.

  Normalerweise folgt die Sprache der Systemeinstellung aus
  Prefs/Locale, es ist also nichts zu tun. Wer sein Workbench englisch
  faehrt, dieses Programm aber deutsch will, setzt eine Variable:

      SetEnv SAVE AmiHomeassistLanguage deutsch

  Gueltig sind die Verzeichnisnamen: deutsch, italiano, espanol (mit
  Tilde). Zurueck auf Englisch:

      UnSetEnv AmiHomeassistLanguage
      Delete ENVARC:AmiHomeassistLanguage

  Beide Zeilen sind noetig - SAVE hat eine Kopie nach ENVARC: gelegt.

  Geraete- und Raumnamen, Einheiten und Zustaende kommen aus Home
  Assistant und sind damit ohnehin in deiner Sprache.

  Gruppenueberschriften eines Dashboards werden beim Anlegen einer
  Seite in Dashboards.prefs geschrieben und behalten die Sprache, in
  der sie entstanden sind. Ein Sprachwechsel schreibt sie nicht um;
  neue Seiten entstehen in der neuen Sprache, und im Editor lassen sich
  alte umbenennen.

  Italienisch und Spanisch stammen nicht von Muttersprachlern. Die
  .ct-Dateien im Source-Verzeichnis sind reiner Text - Korrekturen sind
  willkommen und brauchen keine Neuuebersetzung.


DATEIEN
-------

  ENVARC:AmiHomeassist/AmiHomeassist.prefs   Adresse, Token, Abstand
  ENVARC:AmiHomeassist/Import.prefs          uebernommene Geraete
  ENVARC:AmiHomeassist/Dashboards.prefs      die Seiten

  Alle drei sind Text und notfalls von Hand zu reparieren.

  Der Token steht im Klartext und gilt uneingeschraenkt fuer deine
  Home-Assistant-Installation. Im eigenen Netz ist das vertretbar, aber
  man sollte es wissen.


AUS DER SHELL
-------------

  AmiHomeassistCLI                 Liste nach Raum, gefiltert
  AmiHomeassistCLI ALL             auch die ausgeblendeten Eintraege
  AmiHomeassistCLI DOMAIN=sensor   nur eine Domain
  AmiHomeassistCLI STATES          zeigt den Sparbetrieb
  AmiHomeassistCLI ON=light.kueche
  AmiHomeassistCLI OFF=switch.beamer
  AmiHomeassistCLI TOGGLE=light.tisch

  Einrichten geht auch ohne Oberflaeche:

  AmiHomeassistCLI HOST=http://homeassistant:8123 TOKEN=<token> SAVE


WIE DIE RAEUME HEREINKOMMEN
---------------------------

  Die REST-Schnittstelle von Home Assistant liefert keine Raumzuordnung
  - die steht nur in der Registry hinter der WebSocket-Schnittstelle.
  Statt die nachzubauen, schickt AmiHomeassist ein Jinja-Template an
  /api/template und laesst Home Assistant die Zuordnung selbst
  aufloesen. Zurueck kommen fertige Zeilen. Damit braucht der Amiga
  weder WebSocket noch JSON-Parser.


HERKUNFT UND RECHTE
-------------------

  Die Symbole stammen aus den Material Design Icons, Apache 2.0:
  https://github.com/Templarian/MaterialDesign

  Das Haus-Logo gehoert dem Home-Assistant-Projekt. Home Assistant ist
  ein eigenstaendiges Projekt und hat mit diesem Programm nichts zu
  tun: https://www.home-assistant.io/

  Der Quelltext liegt vollstaendig bei, MIT-Lizenz. Siehe das
  Verzeichnis Source.

  muistubs.c - die ausgelagerten Varargs-Stubs fuer muimaster.library -
  stammt aus dem Projekt amimcp und steht unter Apache 2.0, nicht unter
  MIT. Der Lizenztext liegt in Source/LICENSE-Apache-2.0, die Herkunft
  steht in der Datei selbst:
  https://github.com/thomas-luebker/amimcp

  Getestet auf A500 mit PiStorm (Emu68), OS 3.2.3, MUI 3.8. Ein echter
  68030 steht noch aus.

  Dieses Programm ist vibe-coded. Ich habe beschrieben, was es koennen
  soll, geschrieben hat den Code eine KI, und gelesen, getestet und
  entschieden habe ich. Der Quelltext liegt vollstaendig bei - schau
  selbst hinein.


SELBER UEBERSETZEN
------------------

  Gebaut wird cross mit m68k-amigaos-gcc (AmigaPorts), nicht mehr mit
  SAS/C auf dem Amiga wie noch bei 0.5. Im Verzeichnis Source:

      make

  Zielprofil ist 68020 aufwaerts, mit und ohne FPU - deshalb steht in
  den Uebersetzungsschaltern -mcpu=68020 und sonst nichts. "make
  check-fpu" weist per Disassembler nach, dass keine einzige
  FPU-Instruktion im Binary gelandet ist.

  Zwei Dinge liegen NICHT bei und muessen selbst besorgt werden:

    MUIs eigene Entwickler-Header (libraries/mui.h, proto/muimaster.h,
    clib/muimaster_protos.h, inline/muimaster.h). MUIs Lizenz erlaubt
    nur die Weitergabe des vollstaendigen Originalarchivs, sie duerfen
    hier also nicht mitfahren. Aus dem MUI-Entwicklerarchiv nehmen und
    nach Source/vendor/mui/include/ legen.

    Eine m68k-amigaos-Cross-Toolchain:
    https://github.com/AmigaPorts/m68k-amigaos-gcc

  Die NList-Header liegen dagegen bei, unter Source/mui/ - sie sind
  LGPL. Es sind nur Header; die .mcc-Klassen selbst installiert man wie
  oben beschrieben aus dem Aminet.

  locale.py macht aus strings.cd und catalogs/*.ct die Kataloge und die
  Datei locale_strings.h. Diese Datei liegt fertig bei, ein Bau kommt
  also ohne Python aus.


  Hinweis zur Fehlersuche: eine ausfuehrliche Beschreibung der Fallen,
  die beim Umstieg auf gcc aufgetreten sind, steht im Quelltext - vor
  allem in Makefile und amiloc.c.
