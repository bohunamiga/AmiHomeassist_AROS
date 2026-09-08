/* AmiHomeassist - Dashboards: Datenmodell, Vorbelegung, Datei. */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "amiha.h"
#include "amiloc.h"
#include "dash.h"

extern struct DosLibrary *DOSBase;

#define DASH_DIR   "ENVARC:AmiHomeassist"
#define DASH_FILE  "Dashboards.prefs"

/* ------------------------------------------------------------------ */
/* Anlegen und Freigeben                                               */
/* ------------------------------------------------------------------ */

void dash_init(struct Dash *d)
{
    d->p = NULL;
    d->count = 0;
    d->capacity = 0;
}

void dash_free(struct Dash *d)
{
    int i, j;

    for (i = 0; i < d->count; i++) {
        for (j = 0; j < d->p[i].count; j++) {
            if (d->p[i].g[j].w) {
                free(d->p[i].g[j].w);
            }
        }
        if (d->p[i].g) {
            free(d->p[i].g);
        }
    }
    if (d->p) {
        free(d->p);
    }
    dash_init(d);
}

static void copy_title(char *dst, const char *src)
{
    strncpy(dst, src ? src : "", TITLE_LEN - 1);
    dst[TITLE_LEN - 1] = '\0';
}

struct Page *dash_add_page(struct Dash *d, const char *title, int icon)
{
    struct Page *p;

    if (d->count >= d->capacity) {
        int nc = d->capacity ? d->capacity * 2 : 8;
        struct Page *np = (struct Page *)
            realloc(d->p, (size_t)nc * sizeof(struct Page));
        if (!np) {
            return NULL;
        }
        d->p = np;
        d->capacity = nc;
    }
    p = &d->p[d->count++];
    memset(p, 0, sizeof(*p));
    copy_title(p->title, title);
    p->icon = icon;
    return p;
}

struct Group *page_add_group(struct Page *p, const char *title)
{
    struct Group *g;

    if (p->count >= p->capacity) {
        int nc = p->capacity ? p->capacity * 2 : 4;
        struct Group *ng = (struct Group *)
            realloc(p->g, (size_t)nc * sizeof(struct Group));
        if (!ng) {
            return NULL;
        }
        p->g = ng;
        p->capacity = nc;
    }
    g = &p->g[p->count++];
    memset(g, 0, sizeof(*g));
    copy_title(g->title, title);
    return g;
}

struct Widget *group_add_widget(struct Group *g, int kind, const char *id,
                                const char *label)
{
    struct Widget *w;

    if (g->count >= g->capacity) {
        int nc = g->capacity ? g->capacity * 2 : 8;
        struct Widget *nw = (struct Widget *)
            realloc(g->w, (size_t)nc * sizeof(struct Widget));
        if (!nw) {
            return NULL;
        }
        g->w = nw;
        g->capacity = nc;
    }
    w = &g->w[g->count++];
    memset(w, 0, sizeof(*w));
    w->kind = kind;
    strncpy(w->id, id ? id : "", ID_LEN - 1);
    copy_title(w->label, label);
    return w;
}

void dash_set_title(char *dst, const char *src)
{
    copy_title(dst, src);
}

/* ------------------------------------------------------------------ */
/* Umsortieren und Entfernen                                           */
/* ------------------------------------------------------------------ */

void dash_page_remove(struct Dash *d, int idx)
{
    int j;

    if (idx < 0 || idx >= d->count) {
        return;
    }
    for (j = 0; j < d->p[idx].count; j++) {
        if (d->p[idx].g[j].w) {
            free(d->p[idx].g[j].w);
        }
    }
    if (d->p[idx].g) {
        free(d->p[idx].g);
    }
    memmove(&d->p[idx], &d->p[idx + 1],
            (size_t)(d->count - idx - 1) * sizeof(struct Page));
    d->count--;
}

void dash_page_move(struct Dash *d, int idx, int dir)
{
    struct Page tmp;
    int to = idx + dir;

    if (idx < 0 || idx >= d->count || to < 0 || to >= d->count) {
        return;
    }
    tmp = d->p[idx];
    d->p[idx] = d->p[to];
    d->p[to] = tmp;
}

void page_group_remove(struct Page *p, int idx)
{
    if (idx < 0 || idx >= p->count) {
        return;
    }
    if (p->g[idx].w) {
        free(p->g[idx].w);
    }
    memmove(&p->g[idx], &p->g[idx + 1],
            (size_t)(p->count - idx - 1) * sizeof(struct Group));
    p->count--;
}

void page_group_move(struct Page *p, int idx, int dir)
{
    struct Group tmp;
    int to = idx + dir;

    if (idx < 0 || idx >= p->count || to < 0 || to >= p->count) {
        return;
    }
    tmp = p->g[idx];
    p->g[idx] = p->g[to];
    p->g[to] = tmp;
}

void group_widget_remove(struct Group *g, int idx)
{
    if (idx < 0 || idx >= g->count) {
        return;
    }
    memmove(&g->w[idx], &g->w[idx + 1],
            (size_t)(g->count - idx - 1) * sizeof(struct Widget));
    g->count--;
}

void group_widget_move(struct Group *g, int idx, int dir)
{
    struct Widget tmp;
    int to = idx + dir;

    if (idx < 0 || idx >= g->count || to < 0 || to >= g->count) {
        return;
    }
    tmp = g->w[idx];
    g->w[idx] = g->w[to];
    g->w[to] = tmp;
}

struct Widget *group_insert_widget(struct Group *g, int pos, int kind,
                                   const char *id, const char *label)
{
    struct Widget *w = group_add_widget(g, kind, id, label);
    struct Widget tmp;

    if (!w) {
        return NULL;
    }
    if (pos < 0) {
        pos = 0;
    }
    if (pos >= g->count - 1) {
        return w;                  /* steht schon am Ende */
    }
    tmp = g->w[g->count - 1];
    memmove(&g->w[pos + 1], &g->w[pos],
            (size_t)(g->count - 1 - pos) * sizeof(struct Widget));
    g->w[pos] = tmp;
    return &g->w[pos];
}

/* ------------------------------------------------------------------ */
/* Vorschlag fuer die Darstellung                                      */
/* ------------------------------------------------------------------ */

int widget_kind_for(const struct Entity *e, long *min, long *max)
{
    char dom[24];

    *min = 0;
    *max = 100;

    entity_domain(e->id, dom, sizeof(dom));

    if (strcmp(dom, "light") == 0 || strcmp(dom, "switch") == 0) {
        return WK_TOGGLE;
    }
    if (strcmp(dom, "binary_sensor") == 0) {
        return WK_LAMP;
    }
    if (strcmp(dom, "cover") == 0) {
        return WK_COVER;
    }
    if (strcmp(dom, "climate") == 0) {
        return WK_CLIMATE;
    }

    /* Sensoren: als Balken nur, wenn der Bereich von sich aus feststeht.
     * Bei einer Temperatur waere jede Skala geraten, deshalb Zahl. */
    if (strcmp(e->dclass, "battery") == 0 ||
        strcmp(e->dclass, "humidity") == 0) {
        *min = 0;
        *max = 100;
        return WK_GAUGE;
    }
    if (strcmp(e->dclass, "power") == 0) {
        *min = 0;
        *max = 3000;
        return WK_GAUGE;
    }
    return WK_VALUE;
}

/* Die Reihenfolge muss zur Liste ICONS in mdi.py passen. Unbekannte Raeume
 * bekommen das Haus. */
/* SPRACHUNABHAENGIG - hier NICHT die Katalogsprache einsetzen.
 *
 * Verglichen wird gegen die Raumnamen, die aus Home Assistant kommen, und
 * die stehen in der Sprache des Nutzers - unabhaengig davon, welche
 * Sprache Workbench hat. Ein deutsches Home Assistant an einem englischen
 * Workbench muss weiter passende Symbole bekommen. Die Liste wird deshalb
 * ERWEITERT, nie ausgetauscht: je Symbol ein Eintrag je Sprache, alle
 * werden der Reihe nach probiert.
 *
 * Erster Treffer gewinnt, also darf ein Wort nicht in zwei Zeilen stehen.
 */
struct AreaWord { const char *word; short icon; };

static const struct AreaWord AREA_ICON[] = {
    /* deutsch */
    {"Buero", 0}, {"Bad", 1}, {"Dachboden", 2}, {"Garage", 3},
    {"Garten", 4}, {"Keller", 5}, {"Kueche", 6}, {"Schlafzimmer", 7},
    {"Toilette", 8}, {"Treppe", 9}, {"Waschkueche", 10}, {"Wohnzimmer", 11},
    /* english */
    {"Office", 0}, {"Bathroom", 1}, {"Attic", 2}, {"Loft", 2},
    {"Garden", 4}, {"Yard", 4}, {"Cellar", 5}, {"Basement", 5},
    {"Kitchen", 6}, {"Bedroom", 7}, {"Toilet", 8}, {"Stairs", 9},
    {"Hallway", 9}, {"Laundry", 10}, {"Living room", 11}, {"Lounge", 11},
    /* italiano */
    {"Ufficio", 0}, {"Bagno", 1}, {"Soffitta", 2}, {"Giardino", 4},
    {"Cantina", 5}, {"Cucina", 6}, {"Camera", 7}, {"Camera da letto", 7},
    {"Scale", 9}, {"Lavanderia", 10}, {"Soggiorno", 11}, {"Salotto", 11},
    /* español */
    {"Oficina", 0}, {"Despacho", 0}, {"Bano", 1}, {"Desvan", 2},
    {"Jardin", 4}, {"Sotano", 5}, {"Cocina", 6}, {"Dormitorio", 7},
    {"Aseo", 8}, {"Escalera", 9}, {"Lavadero", 10}, {"Salon", 11}
};
#define AREA_ICON_COUNT ((int)(sizeof(AREA_ICON) / sizeof(AREA_ICON[0])))
#define ICON_NOAREA     12
#define ICON_FALLBACK   13

/* Vergleicht ohne Ruecksicht auf Umlaute: "BÃ¼ro" und "Buero" sind dasselbe,
 * und auf dem Amiga steht in der Liste Latin-1, im Quelltext ASCII. */
static BOOL area_matches(const char *area, const char *plain)
{
    const unsigned char *a = (const unsigned char *)area;
    const char *p = plain;

    while (*a && *p) {
        unsigned char c = *a++;

        if (c == 0xFC || c == 0xDC) {          /* ue */
            if (p[0] != 'u' || p[1] != 'e') return FALSE;
            p += 2;
            continue;
        }
        if (c == 0xE4 || c == 0xC4) {          /* ae */
            if (p[0] != 'a' || p[1] != 'e') return FALSE;
            p += 2;
            continue;
        }
        if (c == 0xF6 || c == 0xD6) {          /* oe */
            if (p[0] != 'o' || p[1] != 'e') return FALSE;
            p += 2;
            continue;
        }
        if (tolower(c) != tolower((unsigned char)*p)) {
            return FALSE;
        }
        p++;
    }
    return (BOOL)(*a == '\0' && *p == '\0');
}

int icon_for_area(const char *area)
{
    int i;

    if (!area || !area[0] || strcmp(area, AREA_NONE) == 0) {
        return ICON_NOAREA;
    }
    for (i = 0; i < AREA_ICON_COUNT; i++) {
        if (area_matches(area, AREA_ICON[i].word)) {
            return AREA_ICON[i].icon;
        }
    }
    return ICON_FALLBACK;
}

/* Innerhalb eines Raums nach Art gruppieren. Alles in einen Topf zu werfen
 * ergibt bei einem Raum mit Lampen, Fenstern und Messwerten eine
 * unuebersichtliche Wand; getrennt liest es sich von selbst. */
/* Als Funktion, nicht als Feld: GetStr() ist kein konstanter Ausdruck.
 * Der Titel wandert beim Anlegen einer Seite in die Dashboard-Datei, ist
 * also ab dann fest - wer spaeter die Sprache wechselt, behaelt seine
 * bestehenden Gruppentitel und kann sie im Editor umbenennen. */
static const char *kind_group(int kind)
{
    static const short ID[WK_COUNT] = {
        MSG_GROUP_SWITCHES,    /* WK_TOGGLE */
        MSG_GROUP_OPENINGS,    /* WK_LAMP   */
        MSG_GROUP_READINGS,    /* WK_VALUE  */
        MSG_GROUP_READINGS,    /* WK_GAUGE  */
        MSG_GROUP_COVERS,      /* WK_COVER  */
        -1,                    /* WK_TEXT - ohne Ueberschrift */
        MSG_GROUP_CLIMATE      /* WK_CLIMATE */
    };

    return (ID[kind] < 0) ? "" : GetStr(ID[kind]);
}

/* Reihenfolge der Kaesten auf einer Seite. Die Heizung steht oben: sie ist
 * das einzige Bedienelement, das man im Winter taeglich anfasst. */
#define KIND_ORDER_COUNT 6
static const int KIND_ORDER[KIND_ORDER_COUNT] = { WK_CLIMATE, WK_TOGGLE,
                                                  WK_COVER, WK_LAMP,
                                                  WK_VALUE, WK_GAUGE };

void dash_generate(struct Dash *d, struct Catalog *c)
{
    int i, k;
    char current[AREA_LEN];

    dash_free(d);
    dash_init(d);

    current[0] = '\0';

    /* Der Katalog ist nach Raum sortiert, also entsteht je Raum eine Seite,
     * ohne dass dafuer sortiert werden muesste. */
    for (i = 0; i < c->count; i++) {
        struct Page *page;
        int start = i;

        if (!c->list[i].selected) {
            continue;
        }
        strcpy(current, c->list[i].area);
        page = dash_add_page(d, current, icon_for_area(current));
        if (!page) {
            return;
        }

        /* Fuer diesen Raum je Art einen Kasten anlegen, in fester
         * Reihenfolge - und value und gauge landen im selben. */
        for (k = 0; k < KIND_ORDER_COUNT; k++) {
            struct Group *grp = NULL;
            int kind_wanted = KIND_ORDER[k];
            int j;

            for (j = start; j < c->count &&
                 strcmp(c->list[j].area, current) == 0; j++) {
                struct Entity *e = &c->list[j];
                long mn, mx;
                int kind;

                if (!e->selected) {
                    continue;
                }
                kind = widget_kind_for(e, &mn, &mx);
                if (kind != kind_wanted) {
                    continue;
                }
                /* value und gauge teilen sich einen Kasten - der zweite
                 * Durchgang haengt sich an den vorhandenen an. */
                if (!grp) {
                    int g;

                    for (g = 0; g < page->count; g++) {
                        if (strcmp(page->g[g].title, kind_group(kind)) == 0) {
                            grp = &page->g[g];
                            break;
                        }
                    }
                    if (!grp) {
                        grp = page_add_group(page, kind_group(kind));
                        if (!grp) {
                            return;
                        }
                    }
                }
                {
                    struct Widget *w = group_add_widget(grp, kind, e->id,
                                                        e->name);
                    if (w) {
                        w->min = mn;
                        w->max = mx;
                    }
                }
            }
        }

        /* Bis zum naechsten Raum weiterspringen. */
        while (i + 1 < c->count &&
               strcmp(c->list[i + 1].area, current) == 0) {
            i++;
        }
    }
}

void dash_mark_used(struct Dash *d, struct Catalog *c)
{
    int i, j, k;

    for (i = 0; i < c->count; i++) {
        c->list[i].selected = FALSE;
    }
    for (i = 0; i < d->count; i++) {
        for (j = 0; j < d->p[i].count; j++) {
            for (k = 0; k < d->p[i].g[j].count; k++) {
                struct Widget *w = &d->p[i].g[j].w[k];
                struct Entity *e;

                if (w->kind == WK_TEXT || w->id[0] == '\0') {
                    continue;
                }
                e = catalog_find(c, w->id);
                if (e) {
                    e->selected = TRUE;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Datei                                                               */
/* ------------------------------------------------------------------ */

static const char *KIND_NAME[WK_COUNT] = {
    "toggle", "lamp", "value", "gauge", "cover", "text", "climate"
};

static int kind_from_name(const char *s)
{
    int i;

    for (i = 0; i < WK_COUNT; i++) {
        if (stricmp(s, KIND_NAME[i]) == 0) {
            return i;
        }
    }
    return -1;
}

BOOL dash_exists(void)
{
    BPTR fh;
    char path[256];

    sprintf(path, "%s/%s", DASH_DIR, DASH_FILE);
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (fh) {
        Close(fh);
        return TRUE;
    }
    return FALSE;
}

int dash_save(struct Dash *d)
{
    BPTR fh, lock;
    char path[256];
    int i, j, k;

    lock = CreateDir((STRPTR)DASH_DIR);
    if (lock) {
        UnLock(lock);
    }

    sprintf(path, "%s/%s", DASH_DIR, DASH_FILE);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        return AH_ENOPREFS;
    }

    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_DASH_HEAD));
    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_DASH_NOTE));
    FPrintf(fh, ";   page \"Titel\" icon <n> ... end\n");
    FPrintf(fh, ";   group \"Titel\" ... end\n");
    FPrintf(fh, ";   <art> <entity> \"Beschriftung\" [min max]\n");
    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_DASH_KINDS));

    for (i = 0; i < d->count; i++) {
        struct Page *p = &d->p[i];

        FPrintf(fh, "page \"%s\" icon %ld\n",
                (LONG)(ULONG)p->title, (LONG)p->icon);
        for (j = 0; j < p->count; j++) {
            struct Group *g = &p->g[j];

            FPrintf(fh, "  group \"%s\"\n", (LONG)(ULONG)g->title);
            for (k = 0; k < g->count; k++) {
                struct Widget *w = &g->w[k];

                if (w->kind == WK_GAUGE) {
                    FPrintf(fh, "    gauge %s \"%s\" %ld %ld\n",
                            (LONG)(ULONG)w->id, (LONG)(ULONG)w->label,
                            (LONG)w->min, (LONG)w->max);
                } else if (w->kind == WK_TEXT) {
                    FPrintf(fh, "    text - \"%s\"\n", (LONG)(ULONG)w->label);
                } else {
                    FPrintf(fh, "    %s %s \"%s\"\n",
                            (LONG)(ULONG)KIND_NAME[w->kind],
                            (LONG)(ULONG)w->id, (LONG)(ULONG)w->label);
                }
            }
            FPrintf(fh, "  end\n");
        }
        FPrintf(fh, "end\n\n");
    }

    Close(fh);
    return AH_OK;
}

/* Holt das naechste Wort. Steht dort ein Anfuehrungszeichen, gilt alles bis
 * zum schliessenden als ein Wort - so duerfen Titel Leerzeichen haben. */
static char *next_word(char **pp, char *out, int outsize)
{
    char *p = *pp;
    int n = 0;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        *pp = p;
        return NULL;
    }

    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (n < outsize - 1) {
                out[n++] = *p;
            }
            p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        while (*p && *p != ' ' && *p != '\t') {
            if (n < outsize - 1) {
                out[n++] = *p;
            }
            p++;
        }
    }
    out[n] = '\0';
    *pp = p;
    return out;
}

int dash_load(struct Dash *d)
{
    BPTR fh;
    char path[256];
    char line[400];
    struct Page *page = NULL;
    struct Group *grp = NULL;

    dash_free(d);
    dash_init(d);

    sprintf(path, "%s/%s", DASH_DIR, DASH_FILE);
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        return AH_ENOPREFS;
    }

    while (FGets(fh, (STRPTR)line, sizeof(line) - 1)) {
        char *p = line;
        char word[TITLE_LEN + ID_LEN];
        char title[TITLE_LEN];
        char id[ID_LEN];

        /* Zeilenende abschneiden */
        {
            char *e = p + strlen(p);
            while (e > p && (e[-1] == '\n' || e[-1] == '\r')) {
                *--e = '\0';
            }
        }
        if (!next_word(&p, word, sizeof(word))) {
            continue;
        }
        if (word[0] == ';' || word[0] == '#') {
            continue;
        }

        if (stricmp(word, "page") == 0) {
            int icon = 0;

            next_word(&p, title, sizeof(title));
            if (next_word(&p, word, sizeof(word)) &&
                stricmp(word, "icon") == 0) {
                if (next_word(&p, word, sizeof(word))) {
                    icon = atoi(word);
                }
            }
            page = dash_add_page(d, title, icon);
            grp = NULL;
        } else if (stricmp(word, "group") == 0) {
            next_word(&p, title, sizeof(title));
            if (page) {
                grp = page_add_group(page, title);
            }
        } else if (stricmp(word, "end") == 0) {
            if (grp) {
                grp = NULL;        /* Ende der Gruppe */
            } else {
                page = NULL;       /* Ende der Seite */
            }
        } else {
            int kind = kind_from_name(word);

            if (kind < 0 || !grp) {
                continue;          /* unbekannt - ueberlesen statt meckern */
            }
            next_word(&p, id, sizeof(id));
            next_word(&p, title, sizeof(title));
            {
                struct Widget *w = group_add_widget(grp, kind,
                                       (kind == WK_TEXT) ? "" : id, title);
                if (w && kind == WK_GAUGE) {
                    if (next_word(&p, word, sizeof(word))) {
                        w->min = atol(word);
                    }
                    if (next_word(&p, word, sizeof(word))) {
                        w->max = atol(word);
                    }
                    if (w->max <= w->min) {
                        w->max = w->min + 100;
                    }
                }
            }
        }
    }

    Close(fh);
    return AH_OK;
}
