AmiHomeassist 0.5
=================

  English below, deutsche Fassung weiter unten.
  Deutsche Fassung ab "AmiHomeassist 0.5 - Deutsch".


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

  4. Want it different? Use "Bearbeiten ..." (Edit).


THE THREE WINDOWS

  Device list   the main window with the sidebar

  Auswahl       which devices the program knows at all. "Vorschlag"
                (suggestion) hides the usual ballast: the per-device
                internet-access switches of a Fritzbox, the Dnd lamps
                of smart sockets, technical helper switches and
                anything unavailable.

  Bearbeiten    create pages, form groups, add devices, choose the
                widget kind and the icon, reorder.


WIDGET KINDS

  Schalter    light and switch - the toggle you click
  Laempchen   binary_sensor - window open or closed
  Zahl        sensor with its unit
  Balken      sensor with a fixed range, per cent or watts
  Rollladen   cover - up, stop, down, and the position
  Text        a caption

  When you add a device the program suggests a kind, from its domain
  and device class. The editor lets you change it.


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

  Tested on an A500 with PiStorm (Emu68), OS 3.2.3, MUI 3.8. A real
  68030 is still to come.


================================================================

AmiHomeassist 0.5 - Deutsch
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
  Text        eine Zwischenueberschrift

  Beim Hinzufuegen schlaegt das Programm eine Art vor, aus Domain und
  Geraeteklasse. Im Editor laesst sie sich aendern.


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

  Getestet auf A500 mit PiStorm (Emu68), OS 3.2.3, MUI 3.8. Ein echter
  68030 steht noch aus.
