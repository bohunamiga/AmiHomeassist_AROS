/* AmiHomeassist - Dashboards.
 *
 * Ein Dashboard ist eine Seite in der Seitenleiste. Es enthaelt Gruppen, eine
 * Gruppe enthaelt Widgets, ein Widget zeigt genau eine Entitaet - oder gar
 * keine, wenn es nur eine Beschriftung ist.
 *
 * Die Trennung von Katalog und Dashboards ist Absicht: der Katalog sagt, was
 * es gibt, die Dashboards sagen, was davon wie angezeigt wird. Ein Geraet darf
 * auf mehreren Seiten stehen, und eine Seite darf Geraete aus mehreren Raeumen
 * mischen - der Anwender ordnet, nicht Home Assistant.
 */

#ifndef DASH_H
#define DASH_H

#include <exec/types.h>
#include "amiha.h"

#define WK_TOGGLE  0    /* light, switch - Schalter zum Klicken */
#define WK_LAMP    1    /* binary_sensor - Laempchen, nicht schaltbar */
#define WK_VALUE   2    /* sensor - Zahl mit Einheit */
#define WK_GAUGE   3    /* sensor - Balken zwischen min und max */
#define WK_COVER   4    /* cover - Auf, Stop, Zu */
#define WK_TEXT    5    /* nur Beschriftung, ohne Entitaet */
#define WK_CLIMATE 6    /* climate - Ist, Soll, waermer/kaelter, Betriebsart */
#define WK_COUNT   7

#define TITLE_LEN  40

struct Widget {
    int  kind;
    char id[ID_LEN];            /* leer bei WK_TEXT */
    char label[TITLE_LEN];
    long min, max;              /* nur WK_GAUGE */
};

struct Group {
    char title[TITLE_LEN];
    struct Widget *w;
    int count, capacity;
};

struct Page {
    char title[TITLE_LEN];
    int  icon;                  /* Bildnummer fuer die Seitenleiste */
    struct Group *g;
    int count, capacity;
};

struct Dash {
    struct Page *p;
    int count, capacity;
};

void dash_init(struct Dash *d);
void dash_free(struct Dash *d);

struct Page   *dash_add_page(struct Dash *d, const char *title, int icon);
struct Group  *page_add_group(struct Page *p, const char *title);
struct Widget *group_add_widget(struct Group *g, int kind, const char *id,
                                const char *label);

/* Wie group_add_widget, setzt den Eintrag aber an eine bestimmte Stelle.
 * pos <= 0 heisst ganz nach vorn, pos >= count ans Ende. */
struct Widget *group_insert_widget(struct Group *g, int pos, int kind,
                                   const char *id, const char *label);

/* Legt aus den uebernommenen Geraeten einen brauchbaren Anfang an: eine Seite
 * je Raum, darin eine Gruppe je Domain. Damit steht nach dem ersten Start
 * etwas Sinnvolles auf dem Schirm, ohne dass jemand einen Editor bedienen
 * muss. */
void dash_generate(struct Dash *d, struct Catalog *c);

/* Schlaegt die Darstellung fuer eine Entitaet vor - aus Domain, Einheit und
 * Geraeteklasse. */
int  widget_kind_for(const struct Entity *e, long *min, long *max);

/* Bildnummer fuer einen Raumnamen - siehe die Liste in mdi.py. */
int  icon_for_area(const char *area);

/* Umsortieren und Entfernen. dir ist -1 fuer hoch, +1 fuer runter; liegt das
 * Ziel ausserhalb, passiert nichts. Alle Funktionen arbeiten auf den flachen
 * Feldern, also nur mit memmove - es gibt keine Zeiger auf einzelne
 * Eintraege, die dabei ungueltig wuerden. Wohl aber auf die Felder selbst:
 * nach einem Entfernen muss die Anzeige neu gebaut werden. */
void dash_page_remove(struct Dash *d, int idx);
void dash_page_move(struct Dash *d, int idx, int dir);
void page_group_remove(struct Page *p, int idx);
void page_group_move(struct Page *p, int idx, int dir);
void group_widget_remove(struct Group *g, int idx);
void group_widget_move(struct Group *g, int idx, int dir);

void dash_set_title(char *dst, const char *src);

int  dash_load(struct Dash *d);
int  dash_save(struct Dash *d);

/* TRUE, wenn eine Datei vorhanden war. Fehlt sie, ist es der erste Start. */
BOOL dash_exists(void);

/* Traegt alle in Dashboards benutzten Entitaeten als selected in den Katalog
 * ein - nur die werden im Takt abgefragt. */
void dash_mark_used(struct Dash *d, struct Catalog *c);

#endif
