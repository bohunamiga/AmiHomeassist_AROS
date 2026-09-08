/* AmiHomeassist - gemeinsamer Kern fuer CLI und MUI-Oberflaeche.
 *
 * Alles, was mit Home Assistant spricht, steckt hier. Die Oberflaeche
 * benutzt nur diese Schnittstelle und weiss nichts von HTTP, Templates oder
 * Zeichensaetzen.
 *
 * Zwei getrennte Abfragen, und das ist der Kern des Entwurfs:
 *
 *   catalog_fetch   holt einmalig ALLE in Frage kommenden Entitaeten mit
 *                   Name, Raum, Einheit und Geraeteklasse. Das ist die
 *                   Grundlage fuer die Auswahl und den Dashboard-Editor.
 *
 *   states_refresh  holt im Sekundentakt NUR die Zustaende der Geraete, die
 *                   tatsaechlich angezeigt werden.
 *
 * Der Unterschied ist nicht akademisch: in einer Anlage mit 941 Entitaeten
 * sind es einmal rund 35 KB gegen laufend keine 2 KB. Auf einem PiStorm faellt
 * das nicht auf, auf einem A500 an einer langsamen Leitung entscheidet es
 * darueber, ob das Programm benutzbar ist.
 */

#include "amiloc.h"

#ifndef AMIHA_H
#define AMIHA_H

#include <exec/types.h>

/* Die laengste Entity-ID in der Testanlage hat 74 Zeichen, also ist 96 kein
 * geratener Wert sondern gemessen plus Reserve. */
#define ID_LEN     96
#define NAME_LEN   64
#define AREA_LEN   40
#define STATE_LEN  24
#define UNIT_LEN   12
#define DCLASS_LEN 20

/* Der Platzhalter fuer Geraete ohne Raum. Das Programm setzt ihn selbst
 * (Home Assistant liefert dann gar nichts), also darf er uebersetzt sein -
 * anders als die Wortliste in dash.c, die gegen HA-Daten vergleicht.
 * Wird beim Erzeugen einer Seite in die Dashboard-Datei geschrieben und
 * ist ab dann fest, genau wie die Gruppentitel. */
#define AREA_NONE  GetStr(MSG_ICON_NOAREA)

/* Die Domains, die geholt werden. */
#define HA_DOMAINS \
    "'light','switch','sensor','binary_sensor','cover','climate'"

/* Temperaturen stehen in ZEHNTELGRAD als ganze Zahl - 23,5 Grad ist 235.
 * Kein Fliesskomma: das Zielprofil laeuft ohne FPU, und der Softfloat-Weg
 * waere fuer eine Anzeige mit einer Nachkommastelle Verschwendung. */
#define TEMP_NONE  (-32768)     /* Wert fehlt oder Geraet ist keine Heizung */
#define MODES_LEN  32           /* "auto,heat" - die Liste aus hvac_modes */

struct Entity {
    char id[ID_LEN];
    char name[NAME_LEN];
    char area[AREA_LEN];
    char state[STATE_LEN];
    char unit[UNIT_LEN];        /* °C, W, % ... leer wenn keine */
    char dclass[DCLASS_LEN];    /* temperature, power, window ... */
    int  pos;                   /* nur cover: Stellung 0..100, sonst -1 */

    /* Nur climate, sonst TEMP_NONE. cur und tgt kommen bei jeder Abfrage
     * frisch, die drei Grenzen stehen einmal im Katalog - die aendert ein
     * Thermostat nicht. */
    short cur, tgt;             /* Ist und Soll in Zehntelgrad */
    short tmin, tmax, tstep;    /* Grenzen und Schrittweite des Sollwerts */
    char  modes[MODES_LEN];     /* hvac_modes, mit Komma getrennt */

    BOOL selected;              /* vom Anwender uebernommen */
};

/* Waechst nach Bedarf. Frueher stand hier ein festes Feld fuer 512 Geraete,
 * das allein 128 KB belegte - auf einer Maschine ohne Fast RAM unhoeflich,
 * und fuer eine grosse Anlage trotzdem zu klein. */
struct Catalog {
    struct Entity *list;
    int count;
    int capacity;
};

struct Prefs {
    char host[128];             /* nur der Rechnername, ohne Schema */
    int  port;
    char token[600];
    int  poll;                  /* Sekunden zwischen zwei Abfragen */
};

#define AH_OK        0
#define AH_ENOPREFS  1          /* Einstellungen fehlen oder unvollstaendig */
#define AH_ENET      2          /* Netzwerk, DNS oder Verbindung */
#define AH_EHTTP     3          /* Server antwortet, aber nicht mit 2xx */
#define AH_EMEM      4

const char *ha_last_error(void);
void ah_trace(const char *what, long value);

int  prefs_load(struct Prefs *p);
int  prefs_save(struct Prefs *p);

/* Die Importliste haelt fest, welche Geraete uebernommen wurden - eine
 * Entity-ID je Zeile. found == 0 heisst: noch nie ausgewaehlt, also erster
 * Start. */
int  import_load(struct Catalog *c, int *found);
int  import_save(struct Catalog *c);

void catalog_init(struct Catalog *c);
void catalog_free(struct Catalog *c);
struct Entity *catalog_find(struct Catalog *c, const char *id);
int  catalog_selected_count(struct Catalog *c);

int  catalog_fetch(struct Prefs *p, struct Catalog *c);

/* Aktualisiert state fuer alle Eintraege mit selected == TRUE. Nichts
 * ausgewaehlt heisst: keine Abfrage, kein Netzverkehr. */
int  states_refresh(struct Prefs *p, struct Catalog *c);

/* service ist "turn_on", "turn_off", "toggle", "open_cover" ... Die Domain
 * wird aus der Entity-ID abgeleitet. */
int  ha_service(struct Prefs *p, const char *entity_id, const char *service);

/* Heizung: Sollwert setzen (in Zehntelgrad) und Betriebsart waehlen. */
int  ha_set_temperature(struct Prefs *p, const char *entity_id, int tenths);
int  ha_set_hvac_mode(struct Prefs *p, const char *entity_id, const char *mode);

/* Die naechste Betriebsart aus e->modes, hinter der jetzigen. Liefert FALSE,
 * wenn das Geraet keine oder nur eine kennt - dann gibt es nichts zu wippen. */
BOOL hvac_next_mode(const struct Entity *e, char *out, int outsize);

/* "23.5" aus 235, auch fuer negative Werte. Fuer Anzeige und JSON. */
void temp_text(int tenths, char *out, int outsize);

/* Home Assistant liefert UTF-8, der Amiga will Latin-1. */
void utf8_to_latin1(char *s);

/* TRUE fuer Eintraege, die in der Vorauswahl nur stoeren. */
BOOL entity_is_noise(const struct Entity *e);

/* Sortiert nach Raum, darin nach Name. AREA_NONE kommt ans Ende. */
void catalog_sort(struct Catalog *c);

/* Die Domain einer Entity-ID, also der Teil vor dem Punkt. */
void entity_domain(const char *id, char *out, int outsize);

#endif
