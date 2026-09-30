/* AmiHomeassist - Dashboard-Editor. */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef IPTR
typedef ULONG IPTR;
#endif
#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) \
    ((ULONG)(a)<<24 | (ULONG)(b)<<16 | (ULONG)(c)<<8 | (ULONG)(d))
#endif

#include "mui/NList_mcc.h"
#include "mui/NListview_mcc.h"

#include "amiha.h"
#include "amiloc.h"
#include "dash.h"
#include "edit.h"
#include "icons.h"

extern struct DosLibrary *DOSBase;

/* gui.c oeffnet beide Basen; hier fehlt nur die Bekanntmachung. */
extern struct IntuitionBase *IntuitionBase;
extern struct Library *MUIMasterBase;

enum {
    E_PAGECLICK = EDIT_ID_FIRST,
    E_PAGENEW, E_PAGEDEL, E_PAGEUP, E_PAGEDOWN,
    E_ROWCLICK, E_GROUPNEW, E_ADD, E_DEL, E_UP, E_DOWN,
    E_RENAME, E_ICON, E_KIND,
    E_PAGEDRAG, E_ROWDRAG,
    E_SAVE, E_CLOSE,
    E_PICKADD, E_PICKCLOSE, E_PICKFIND
};

static struct Dash    *g_d;
static struct Catalog *g_c;

static Object *g_win, *g_pages, *g_rows, *g_name, *g_icon, *g_kind;
static Object *g_iconview;
static Object *g_pick_win, *g_pick_list, *g_pick_find;

/* Eine Zeile der Inhaltsliste zeigt entweder eine Gruppe (wi < 0) oder ein
 * Widget darin. Das Feld wird bei jedem Fuellen neu aufgebaut. */
struct Row {
    int gi;
    int wi;
};

static struct Row *g_row = NULL;
static int g_row_count = 0;
static int g_row_cap = 0;

#define ROWTEXT_LEN (TITLE_LEN + 32)
static char *g_rowtext = NULL;
#define ROWTEXT(i) (g_rowtext + (long)(i) * ROWTEXT_LEN)

#define PAGETEXT_LEN (TITLE_LEN + 16)
static char *g_pagetext = NULL;
static int   g_pagetext_cap = 0;
#define PAGETEXT(i) (g_pagetext + (long)(i) * PAGETEXT_LEN)

#define PICKTEXT_LEN (AREA_LEN + NAME_LEN + 8)
static char *g_picktext = NULL;
static int   g_picktext_cap = 0;
#define PICKTEXT(i) (g_picktext + (long)(i) * PICKTEXT_LEN)

/* Die Auswahlliste zeigt nur brauchbare Eintraege, sonst sucht man sich in
 * 643 Zeilen tot. Diese Abbildung fuehrt von der Zeile zum Katalogeintrag. */
static int *g_pickmap = NULL;
static int  g_pickcount = 0;

/* Das abschliessende NULL ist Pflicht: MUIs Cycle-Gadget liest die Liste,
 * bis es eine NULL findet. Ohne die laeuft es ueber das Feld hinaus - und
 * weigert sich dann irgendwann, das Fenster zu oeffnen. */
#define ICON_TEXT_COUNT 36

/* Beide Listen werden zur Laufzeit gefuellt, weil GetStr() kein
 * konstanter Ausdruck ist. MUIO_Cycle merkt sich nur den Zeiger auf das
 * Feld - es muss also statisch sein und darf nicht auf dem Stapel liegen.
 * locale_lists_init() wird einmal vor dem Aufbau des Fensters gerufen. */
/* Das Diagramm steht zweimal in der Auswahl - Tage und Monate -, damit
 * es keine eigenen Knoepfe fuer den Zeitraum braucht. Eintrag KIND_ENTRIES-2
 * ist WK_CHART mit Tagen, der letzte WK_CHART mit Monaten. */
#define KIND_ENTRIES (WK_COUNT + 1)
static const char *KIND_TEXT[KIND_ENTRIES + 1];
static const char *ICON_TEXT[ICON_TEXT_COUNT + 1];

static const short KIND_MSG[WK_COUNT] = {
    MSG_KIND_TOGGLE, MSG_KIND_LAMP, MSG_KIND_VALUE,
    MSG_KIND_GAUGE,  MSG_KIND_COVER, MSG_KIND_TEXT,
    MSG_KIND_CLIMATE, MSG_KIND_CHART
};

/* Welcher Eintrag der Auswahl gehoert zu diesem Widget? */
static int kind_entry(const struct Widget *w)
{
    if (w->kind == WK_CHART && w->min == AH_PERIOD_MONTH) {
        return KIND_ENTRIES - 1;
    }
    return w->kind;
}

/* Muss zur Reihenfolge der Liste ICONS in mdi.py passen. */
static const short ICON_MSG[ICON_TEXT_COUNT] = {
    MSG_ICON_OFFICE,   MSG_ICON_BATH,      MSG_ICON_ATTIC,
    MSG_ICON_GARAGE,   MSG_ICON_GARDEN,    MSG_ICON_CELLAR,
    MSG_ICON_KITCHEN,  MSG_ICON_BEDROOM,   MSG_ICON_TOILET,
    MSG_ICON_STAIRS,   MSG_ICON_LAUNDRY,   MSG_ICON_LIVING,
    MSG_ICON_NOAREA,   MSG_ICON_HOUSE,     MSG_ICON_OVERVIEW,
    MSG_ICON_ENERGY,   MSG_ICON_SOLAR,     MSG_ICON_BATTERY,
    MSG_ICON_TEMPERATURE, MSG_ICON_HUMIDITY, MSG_ICON_WEATHER,
    MSG_ICON_LIGHT,    MSG_ICON_SOCKET,    MSG_ICON_WINDOW,
    MSG_ICON_DOOR,     MSG_ICON_LOCK,      MSG_ICON_SECURITY,
    MSG_ICON_CAR,      MSG_ICON_TV,        MSG_ICON_MUSIC,
    MSG_ICON_NETWORK,  MSG_ICON_BLIND,     MSG_ICON_FAN,
    MSG_ICON_TIME,     MSG_ICON_TOOL,      MSG_ICON_PRINTER
};

static void locale_lists_init(void)
{
    int i;

    for (i = 0; i < WK_COUNT; i++) {
        KIND_TEXT[i] = GetStr(KIND_MSG[i]);
    }
    KIND_TEXT[WK_COUNT] = GetStr(MSG_KIND_CHART_MONTH);
    KIND_TEXT[KIND_ENTRIES] = NULL;  /* MUIO_Cycle liest bis zur NULL */

    for (i = 0; i < ICON_TEXT_COUNT; i++) {
        ICON_TEXT[i] = GetStr(ICON_MSG[i]);
    }
    ICON_TEXT[ICON_TEXT_COUNT] = NULL;
}

/* ------------------------------------------------------------------ */

static int cur_page(void)
{
    LONG n = -1;

    get(g_pages, MUIA_NList_Active, &n);
    if (n < 0 || n >= g_d->count) {
        return -1;
    }
    return (int)n;
}

static int cur_row(void)
{
    LONG n = -1;

    get(g_rows, MUIA_NList_Active, &n);
    if (n < 0 || n >= g_row_count) {
        return -1;
    }
    return (int)n;
}

static BOOL rows_room(int n)
{
    if (n <= g_row_cap) {
        return TRUE;
    }
    if (g_row) {
        free(g_row);
    }
    if (g_rowtext) {
        free(g_rowtext);
    }
    g_row = (struct Row *)malloc((size_t)n * sizeof(struct Row));
    g_rowtext = malloc((size_t)n * ROWTEXT_LEN);
    if (!g_row || !g_rowtext) {
        g_row_cap = 0;
        return FALSE;
    }
    g_row_cap = n;
    return TRUE;
}

static void fill_pages(void)
{
    int i;

    if (g_d->count > g_pagetext_cap) {
        if (g_pagetext) {
            free(g_pagetext);
        }
        g_pagetext = malloc((size_t)(g_d->count + 8) * PAGETEXT_LEN);
        g_pagetext_cap = g_pagetext ? g_d->count + 8 : 0;
        if (!g_pagetext) {
            return;
        }
    }

    set(g_pages, MUIA_NList_Quiet, TRUE);
    DoMethod(g_pages, MUIM_NList_Clear);
    for (i = 0; i < g_d->count; i++) {
        sprintf(PAGETEXT(i), "%s", g_d->p[i].title);
        DoMethod(g_pages, MUIM_NList_InsertSingle, PAGETEXT(i),
                 MUIV_NList_Insert_Bottom);
    }
    set(g_pages, MUIA_NList_Quiet, FALSE);
}

static void fill_rows(void)
{
    int pi = cur_page();
    int j, k, n = 0;
    int need = 1;

    set(g_rows, MUIA_NList_Quiet, TRUE);
    DoMethod(g_rows, MUIM_NList_Clear);
    g_row_count = 0;

    if (pi < 0) {
        set(g_rows, MUIA_NList_Quiet, FALSE);
        return;
    }

    for (j = 0; j < g_d->p[pi].count; j++) {
        need += 1 + g_d->p[pi].g[j].count;
    }
    if (!rows_room(need)) {
        set(g_rows, MUIA_NList_Quiet, FALSE);
        return;
    }

    for (j = 0; j < g_d->p[pi].count; j++) {
        struct Group *g = &g_d->p[pi].g[j];

        g_row[n].gi = j;
        g_row[n].wi = -1;
        sprintf(ROWTEXT(n), "\33b%s", g->title);
        DoMethod(g_rows, MUIM_NList_InsertSingle, ROWTEXT(n),
                 MUIV_NList_Insert_Bottom);
        n++;

        for (k = 0; k < g->count; k++) {
            g_row[n].gi = j;
            g_row[n].wi = k;
            sprintf(ROWTEXT(n), "    %-26s %s", g->w[k].label,
                    KIND_TEXT[kind_entry(&g->w[k])]);
            DoMethod(g_rows, MUIM_NList_InsertSingle, ROWTEXT(n),
                     MUIV_NList_Insert_Bottom);
            n++;
        }
    }
    g_row_count = n;
    set(g_rows, MUIA_NList_Quiet, FALSE);
}

/* Uebernimmt Name, Symbol und Art der aktuellen Auswahl in die Gadgets. */
static void show_selection(void)
{
    int pi = cur_page();
    int r = cur_row();

    if (pi < 0) {
        return;
    }
    if (r < 0) {
        set(g_name, MUIA_String_Contents, g_d->p[pi].title);
        set(g_icon, MUIA_Cycle_Active, (LONG)g_d->p[pi].icon);
        set(g_iconview, MUIA_Group_ActivePage, (LONG)g_d->p[pi].icon);
        return;
    }
    {
        struct Group *g = &g_d->p[pi].g[g_row[r].gi];

        if (g_row[r].wi < 0) {
            set(g_name, MUIA_String_Contents, g->title);
        } else {
            struct Widget *w = &g->w[g_row[r].wi];

            set(g_name, MUIA_String_Contents, w->label);
            set(g_kind, MUIA_Cycle_Active, (LONG)kind_entry(w));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Geraeteauswahl                                                      */
/* ------------------------------------------------------------------ */

static void fill_picker(void)
{
    int i, n = 0;
    char *find = NULL;

    if (g_c->count > g_picktext_cap) {
        if (g_picktext) {
            free(g_picktext);
        }
        if (g_pickmap) {
            free(g_pickmap);
        }
        g_picktext = malloc((size_t)g_c->count * PICKTEXT_LEN);
        g_pickmap = (int *)malloc((size_t)g_c->count * sizeof(int));
        g_picktext_cap = (g_picktext && g_pickmap) ? g_c->count : 0;
        if (!g_picktext_cap) {
            return;
        }
    }

    get(g_pick_find, MUIA_String_Contents, &find);
    if (!find) {
        find = "";
    }

    set(g_pick_list, MUIA_NList_Quiet, TRUE);
    DoMethod(g_pick_list, MUIM_NList_Clear);
    for (i = 0; i < g_c->count; i++) {
        const struct Entity *e = &g_c->list[i];

        /* Ohne Suchbegriff bleibt der Ballast draussen (Fritzbox-Schalter,
         * Unerreichbares). Wer sucht, sucht gezielt - dann alles zeigen,
         * worauf der Begriff passt: Raum, Name oder Entity-ID. */
        if (*find) {
            if (!text_contains(e->name, find) && !text_contains(e->area, find) &&
                    !text_contains(e->id, find)) {
                continue;
            }
        } else if (entity_is_noise(e)) {
            continue;
        }
        g_pickmap[n] = i;
        sprintf(PICKTEXT(n), "%-14s %s", g_c->list[i].area, g_c->list[i].name);
        DoMethod(g_pick_list, MUIM_NList_InsertSingle, PICKTEXT(n),
                 MUIV_NList_Insert_Bottom);
        n++;
    }
    g_pickcount = n;
    set(g_pick_list, MUIA_NList_Quiet, FALSE);
}

static BOOL picker_add(void)
{
    LONG sel = -1;
    int pi = cur_page();
    int r = cur_row();
    int gi = 0;
    struct Entity *e;
    long mn, mx;

    get(g_pick_list, MUIA_NList_Active, &sel);
    if (sel < 0 || sel >= g_pickcount || pi < 0) {
        return FALSE;
    }
    if (g_d->p[pi].count == 0) {
        if (!page_add_group(&g_d->p[pi], GetStr(MSG_ED_DEVICES))) {
            return FALSE;
        }
    }
    /* In die Gruppe der aktuellen Zeile, sonst in die erste. */
    if (r >= 0) {
        gi = g_row[r].gi;
    }
    if (gi >= g_d->p[pi].count) {
        gi = 0;
    }

    e = &g_c->list[g_pickmap[sel]];
    {
        int kind = widget_kind_for(e, &mn, &mx);
        struct Widget *w = group_add_widget(&g_d->p[pi].g[gi], kind,
                                            e->id, e->name);
        if (!w) {
            return FALSE;
        }
        w->min = mn;
        w->max = mx;
    }
    fill_rows();
    return TRUE;
}

/* Verschiebt ein Widget. Am Rand einer Gruppe wandert es in die
 * benachbarte - sonst kaeme man aus einer falsch geratenen Gruppe nur mit
 * Loeschen und neu Hinzufuegen wieder heraus. */
static void move_widget(int pi, int gi, int wi, int dir)
{
    struct Page *p = &g_d->p[pi];
    struct Group *g = &p->g[gi];
    struct Widget w;

    if (dir < 0 && wi == 0) {
        if (gi == 0) {
            return;
        }
        w = g->w[0];
        {
            struct Widget *nw = group_add_widget(&p->g[gi - 1], w.kind,
                                                 w.id, w.label);
            if (!nw) {
                return;
            }
            nw->min = w.min;
            nw->max = w.max;
        }
        group_widget_remove(g, 0);
        return;
    }
    if (dir > 0 && wi == g->count - 1) {
        if (gi >= p->count - 1) {
            return;
        }
        w = g->w[wi];
        {
            struct Widget *nw = group_insert_widget(&p->g[gi + 1], 0, w.kind,
                                                    w.id, w.label);
            if (!nw) {
                return;
            }
            nw->min = w.min;
            nw->max = w.max;
        }
        group_widget_remove(g, wi);
        return;
    }
    group_widget_move(g, wi, dir);
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Ziehen und Ablegen                                                  */
/* ------------------------------------------------------------------ */

/* Verschiebt ein Element eines Feldes von 'from' nach 'to' (Zielindex im
 * fertigen Feld), ohne Speicher anzufordern - ein Tausch auf der Stelle.
 * Die Zeiger in Gruppen und Seiten wandern mit, nichts wird kopiert. */
static void array_move(void *base, int size, int from, int to)
{
    char tmp[sizeof(struct Page) > sizeof(struct Group) ?
             (sizeof(struct Page) > sizeof(struct Widget) ?
              sizeof(struct Page) : sizeof(struct Widget)) :
             (sizeof(struct Group) > sizeof(struct Widget) ?
              sizeof(struct Group) : sizeof(struct Widget))];
    char *b = (char *)base;

    if (from == to) {
        return;
    }
    memcpy(tmp, b + (long)from * size, size);
    if (from < to) {
        memmove(b + (long)from * size, b + (long)(from + 1) * size,
                (size_t)(to - from) * size);
    } else {
        memmove(b + (long)(to + 1) * size, b + (long)to * size,
                (size_t)(from - to) * size);
    }
    memcpy(b + (long)to * size, tmp, size);
}

/* Welche Zeile (alter Index) steht jetzt an Position 'pos' der Liste?
 * NList speichert unsere Textzeiger unveraendert, daraus folgt der Index. */
static int row_at(Object *list, LONG pos, char *base, int len, int count)
{
    char *p = NULL;
    long n;

    DoMethod(list, MUIM_NList_GetEntry, pos, &p);
    if (!p || p < base) {
        return -1;
    }
    n = (long)(p - base) / len;
    return (n >= 0 && n < count) ? (int)n : -1;
}

/* Eine Seite wurde in der Seitenliste gezogen. */
static BOOL drag_pages(void)
{
    LONG ins = -1;
    int from;

    get(g_pages, MUIA_NList_DragSortInsert, &ins);
    from = row_at(g_pages, ins, g_pagetext, PAGETEXT_LEN, g_d->count);
    if (from < 0 || ins < 0 || ins >= g_d->count) {
        fill_pages();                /* Liste wieder mit dem Modell abgleichen */
        return FALSE;
    }
    array_move(g_d->p, sizeof(struct Page), from, (int)ins);
    fill_pages();
    set(g_pages, MUIA_NList_Active, ins);
    fill_rows();
    return TRUE;
}

/* Eine Zeile der Inhaltsliste wurde gezogen.
 *
 * Die Liste ist flach: Ueberschrift, ihre Geraete, naechste Ueberschrift ...
 * Ein Geraet gehoert nach dem Ablegen zu der Ueberschrift, die ueber ihm
 * steht. Eine Ueberschrift nimmt ihren ganzen Kasten mit - sonst wuerde sie
 * beim Hochziehen die Geraete des Kastens darueber einsammeln, und das
 * erwartet niemand. */
static BOOL drag_rows(int *newrow)
{
    int pi = cur_page();
    LONG ins = -1;
    int from, i, pos;
    int tg = -1, tpos = 0, heads = 0;

    get(g_rows, MUIA_NList_DragSortInsert, &ins);
    from = row_at(g_rows, ins, g_rowtext, ROWTEXT_LEN, g_row_count);
    *newrow = -1;
    if (pi < 0 || from < 0 || ins < 0 || ins >= g_row_count) {
        fill_rows();
        return FALSE;
    }

    /* Die neue Reihenfolge ist die alte ohne 'from', mit 'from' an 'ins'.
     * Gezaehlt wird nur, was VOR der Ablegestelle steht. */
    pos = 0;
    for (i = 0; i < g_row_count && pos < ins; i++) {
        if (i == from) {
            continue;
        }
        if (g_row[i].wi < 0) {
            tg = g_row[i].gi;
            tpos = 0;
            heads++;
        } else {
            tpos++;
        }
        pos++;
    }

    if (g_row[from].wi < 0) {
        /* Kasten: er landet hinter so vielen Kaesten, wie Ueberschriften
         * vor ihm stehen. */
        int gfrom = g_row[from].gi;

        array_move(g_d->p[pi].g, sizeof(struct Group), gfrom, heads);
        fill_rows();
        for (i = 0; i < g_row_count; i++) {
            if (g_row[i].wi < 0 && g_row[i].gi == heads) {
                *newrow = i;
            }
        }
        return TRUE;
    }

    {
        struct Page *p = &g_d->p[pi];
        int gfrom = g_row[from].gi;
        int wfrom = g_row[from].wi;

        if (tg < 0) {
            /* Ueber die erste Ueberschrift gezogen: in den ersten Kasten,
             * ganz nach oben. */
            tg = 0;
            tpos = 0;
        }
        if (tg == gfrom) {
            /* Innerhalb des Kastens: tpos zaehlt schon ohne das gezogene
             * Geraet, ist also direkt der Zielindex. */
            array_move(p->g[gfrom].w, sizeof(struct Widget), wfrom, tpos);
        } else {
            struct Widget w = p->g[gfrom].w[wfrom];
            struct Widget *nw = group_insert_widget(&p->g[tg], tpos, w.kind,
                                                    w.id, w.label);
            if (!nw) {
                fill_rows();
                return FALSE;
            }
            nw->min = w.min;
            nw->max = w.max;
            group_widget_remove(&p->g[gfrom], wfrom);
        }
        fill_rows();
        for (i = 0; i < g_row_count; i++) {
            if (g_row[i].gi == tg && g_row[i].wi == tpos) {
                *newrow = i;
            }
        }
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */

/* Alle Symbole in einer Seitengruppe. Sichtbar ist das gewaehlte - so sieht
 * man beim Blaettern durch die Liste sofort, was man bekommt, ohne dass zur
 * Laufzeit Bilddaten getauscht werden muessten. */
static Object *one_icon(int n)
{
    return MUI_NewObject(MUIC_Bodychunk,
        MUIA_FixWidth,              24,
        MUIA_FixHeight,             24,
        MUIA_Bitmap_Width,          24,
        MUIA_Bitmap_Height,         24,
        MUIA_Bodychunk_Depth,       4,
        MUIA_Bodychunk_Body,        (UBYTE *)mdi_icon(n)->body,
        MUIA_Bodychunk_Compression, 0,
        MUIA_Bodychunk_Masking,     0,
        MUIA_Bitmap_SourceColors,   (ULONG *)mdi_colors,
        MUIA_Bitmap_Transparent,    0,
        TAG_DONE);
}

static Object *icon_preview(void)
{
    Object *grp;
    int i;

    /* Die Gruppe braucht schon beim Anlegen ein Kind - eine leere Gruppe
     * legt MUI nicht an, und ein NULL-Kind reisst das ganze Fenster mit. */
    grp = MUI_NewObject(MUIC_Group,
        MUIA_Group_PageMode, TRUE,
        MUIA_FixWidth,       24,
        MUIA_FixHeight,      24,
        MUIA_Group_Child,    one_icon(0),
        TAG_DONE);

    if (!grp) {
        return NULL;
    }
    for (i = 1; i < MDI_COUNT; i++) {
        Object *im = MUI_NewObject(MUIC_Bodychunk,
            MUIA_FixWidth,              24,
            MUIA_FixHeight,             24,
            MUIA_Bitmap_Width,          24,
            MUIA_Bitmap_Height,         24,
            MUIA_Bodychunk_Depth,       4,
            MUIA_Bodychunk_Body,        (UBYTE *)mdi_icon(i)->body,
            MUIA_Bodychunk_Compression, 0,
            MUIA_Bodychunk_Masking,     0,
            MUIA_Bitmap_SourceColors,   (ULONG *)mdi_colors,
            MUIA_Bitmap_Transparent,    0,
            TAG_DONE);

        if (im) {
            DoMethod(grp, OM_ADDMEMBER, im);
        }
    }
    return grp;
}

/* "Suche:" - rechtsbuendig mit Doppelpunkt wie alle Beschriftungen. */
static Object *find_label(void)
{
    char buf[48];

    sprintf(buf, "%.44s:", GetStr(MSG_ED_LBL_FIND));
    return MUI_MakeObject(MUIO_Label, buf, 0);
}

Object *editor_build(Object *app, struct Dash *d, struct Catalog *c)
{
    Object *b_pnew, *b_pdel, *b_pup, *b_pdown;
    Object *b_gnew, *b_add, *b_del, *b_up, *b_down;
    Object *b_ren, *b_icon, *b_kind, *b_save, *b_close;
    Object *b_padd, *b_pclose;

    g_d = d;
    g_c = c;

    /* Muss vor dem Fensteraufbau stehen: MUIO_Cycle merkt sich nur den
     * Zeiger auf das Beschriftungsfeld, liest es aber erst beim Zeichnen. */
    locale_lists_init();

    g_win = MUI_NewObject(MUIC_Window,
        MUIA_Window_Title,  (char *)GetStr(MSG_ED_TITLE),
        MUIA_Window_ID,     MAKE_ID('A','H','A','4'),
        MUIA_Window_Width,  MUIV_Window_Width_Visible(60),
        MUIA_Window_Height, MUIV_Window_Height_Visible(60),
        MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,

            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,

                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_HorizWeight, 35,
                    MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                        MUIA_Text_Contents, (char *)GetStr(MSG_ED_PAGES), TAG_DONE),
                    MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                        MUIA_NListview_NList, g_pages =
                            MUI_NewObject(MUIC_NList,
                                MUIA_NList_Input,          TRUE,
                                MUIA_NList_DragSortable,   TRUE,
                                MUIA_NList_DragType,       MUIV_NList_DragType_Immediate,
                                MUIA_NList_ShowDropMarks,  TRUE,
                                TAG_DONE),
                        TAG_DONE),
                    MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                        MUIA_Group_Horiz, TRUE,
                        MUIA_Group_Child, b_pnew =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_NEW)),
                        MUIA_Group_Child, b_pdel =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_REMOVE)),
                        MUIA_Group_Child, b_pup =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_UP)),
                        MUIA_Group_Child, b_pdown =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_DOWN)),
                        TAG_DONE),
                    TAG_DONE),

                MUIA_Group_Child, MUI_NewObject(MUIC_Balance, TAG_DONE),

                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_HorizWeight, 65,
                    MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                        MUIA_Text_Contents, (char *)GetStr(MSG_ED_CONTENT), TAG_DONE),
                    MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                        MUIA_NListview_NList, g_rows =
                            MUI_NewObject(MUIC_NList,
                                MUIA_NList_Input,          TRUE,
                                MUIA_NList_DragSortable,   TRUE,
                                MUIA_NList_DragType,       MUIV_NList_DragType_Immediate,
                                MUIA_NList_ShowDropMarks,  TRUE,
                                TAG_DONE),
                        TAG_DONE),
                    MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                        MUIA_Group_Horiz, TRUE,
                        MUIA_Group_Child, b_gnew =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_GROUP)),
                        MUIA_Group_Child, b_add =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_DEVICE)),
                        MUIA_Group_Child, b_del =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_REMOVE)),
                        MUIA_Group_Child, b_up =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_UP)),
                        MUIA_Group_Child, b_down =
                            MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_DOWN)),
                        TAG_DONE),
                    TAG_DONE),
                TAG_DONE),

            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                MUIA_Frame,      MUIV_Frame_Group,
                MUIA_FrameTitle, (char *)GetStr(MSG_ED_SELECTED),
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, MUI_MakeObject(MUIO_Label,
                                          (char *)GetStr(MSG_ED_LBL_NAME), 0),
                    MUIA_Group_Child, g_name = MUI_NewObject(MUIC_String,
                        MUIA_String_MaxLen, TITLE_LEN,
                        MUIA_Frame,         MUIV_Frame_String,
                        TAG_DONE),
                    MUIA_Group_Child, b_ren =
                        MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_RENAME)),
                    TAG_DONE),
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, g_iconview = icon_preview(),
                    MUIA_Group_Child, g_icon =
                        MUI_MakeObject(MUIO_Cycle, (char *)GetStr(MSG_ED_LBL_ICON),
                                       (char **)ICON_TEXT),
                    MUIA_Group_Child, b_icon =
                        MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_SET)),
                    MUIA_Group_Child, g_kind =
                        MUI_MakeObject(MUIO_Cycle, (char *)GetStr(MSG_ED_LBL_KIND),
                                       (char **)KIND_TEXT),
                    MUIA_Group_Child, b_kind =
                        MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_SET)),
                    TAG_DONE),
                TAG_DONE),

            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, b_save =
                    MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_BT_SAVE)),
                MUIA_Group_Child, b_close =
                    MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_BT_CLOSE)),
                TAG_DONE),
            TAG_DONE),
        TAG_DONE);

    g_pick_win = MUI_NewObject(MUIC_Window,
        MUIA_Window_Title,  (char *)GetStr(MSG_ED_ADDDEVICE),
        MUIA_Window_ID,     MAKE_ID('A','H','A','5'),
        MUIA_Window_Width,  MUIV_Window_Width_Visible(40),
        MUIA_Window_Height, MUIV_Window_Height_Visible(60),
        MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,
            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, find_label(),
                MUIA_Group_Child, g_pick_find = MUI_NewObject(MUIC_String,
                    MUIA_String_MaxLen, 40,
                    MUIA_Frame,         MUIV_Frame_String,
                    MUIA_CycleChain,    1,
                    TAG_DONE),
                TAG_DONE),
            MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                MUIA_NListview_NList, g_pick_list = MUI_NewObject(MUIC_NList,
                    MUIA_NList_Input, TRUE, TAG_DONE),
                MUIA_CycleChain, 1,
                TAG_DONE),
            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, b_padd =
                    MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_ED_ADD)),
                MUIA_Group_Child, b_pclose =
                    MUI_MakeObject(MUIO_Button, (char *)GetStr(MSG_BT_CLOSE)),
                TAG_DONE),
            TAG_DONE),
        TAG_DONE);

    if (!g_win || !g_pick_win) {
        return NULL;
    }
    /* Beim Oeffnen gleich ins Suchfeld tippen koennen. */
    set(g_pick_win, MUIA_Window_ActiveObject, g_pick_find);

    DoMethod(app, OM_ADDMEMBER, g_win);
    DoMethod(app, OM_ADDMEMBER, g_pick_win);

    DoMethod(g_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             g_win, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(g_pick_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             g_pick_win, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    DoMethod(g_pages, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_PAGECLICK);
    DoMethod(g_rows, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_ROWCLICK);

    /* Nach dem Ziehen hat NList die Zeile schon umgehaengt - im Modell
     * aber steht noch alles am alten Platz. Das holen drag_rows() und
     * drag_pages() nach. */
    DoMethod(g_pages, MUIM_Notify, MUIA_NList_DragSortInsert, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_PAGEDRAG);
    DoMethod(g_rows, MUIM_Notify, MUIA_NList_DragSortInsert, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_ROWDRAG);

    DoMethod(b_pnew, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PAGENEW);
    DoMethod(b_pdel, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PAGEDEL);
    DoMethod(b_pup, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PAGEUP);
    DoMethod(b_pdown, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PAGEDOWN);

    DoMethod(b_gnew, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_GROUPNEW);
    DoMethod(b_add, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_ADD);
    DoMethod(b_del, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_DEL);
    DoMethod(b_up, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_UP);
    DoMethod(b_down, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_DOWN);

    DoMethod(b_ren, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_RENAME);
    DoMethod(g_name, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_RENAME);
    DoMethod(b_icon, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_ICON);
    /* Beim Blaettern gleich zeigen, was gewaehlt ist. */
    DoMethod(g_icon, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             g_iconview, 3, MUIM_Set, MUIA_Group_ActivePage,
             MUIV_TriggerValue);
    DoMethod(b_kind, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_KIND);

    DoMethod(b_save, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_SAVE);
    DoMethod(b_close, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_CLOSE);
    DoMethod(b_padd, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PICKADD);
    /* Doppelklick fuegt hinzu wie der Knopf; jede Eingabe filtert neu. */
    DoMethod(g_pick_list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_PICKADD);
    DoMethod(g_pick_find, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, E_PICKFIND);
    DoMethod(b_pclose, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PICKCLOSE);

    return g_win;
}

/* Oeffnet auf der Seite, die im Hauptfenster gerade zu sehen ist - wer im
 * Wohnzimmer auf "Bearbeiten" drueckt, will das Wohnzimmer bearbeiten. */
void editor_open(int page)
{
    if (!g_win) {
        return;                    /* Fenster kam nicht zustande */
    }
    fill_pages();
    if (g_d->count) {
        if (page < 0 || page >= g_d->count) {
            page = 0;
        }
        set(g_pages, MUIA_NList_Active, (LONG)page);
    }
    fill_rows();
    set(g_win, MUIA_Window_Open, TRUE);
}

BOOL editor_handle(ULONG id, BOOL *changed)
{
    int pi, r;

    if (id < EDIT_ID_FIRST || id > EDIT_ID_LAST) {
        return FALSE;
    }
    *changed = FALSE;
    pi = cur_page();
    r = cur_row();

    switch (id) {
        case E_PAGECLICK:
            fill_rows();
            show_selection();
            break;

        case E_ROWCLICK:
            show_selection();
            break;

        case E_PAGENEW:
            if (dash_add_page(g_d, GetStr(MSG_ED_NEWPAGE), 13)) {
                page_add_group(&g_d->p[g_d->count - 1], GetStr(MSG_ED_DEVICES));
                fill_pages();
                set(g_pages, MUIA_NList_Active, (LONG)(g_d->count - 1));
                fill_rows();
                *changed = TRUE;
            }
            break;

        case E_PAGEDEL:
            if (pi >= 0) {
                dash_page_remove(g_d, pi);
                fill_pages();
                /* Auf die Nachbarseite, nicht ins Leere - sonst stuende im
                 * Namensfeld noch die geloeschte Seite. */
                if (g_d->count) {
                    set(g_pages, MUIA_NList_Active,
                        (LONG)(pi < g_d->count ? pi : g_d->count - 1));
                }
                fill_rows();
                show_selection();
                *changed = TRUE;
            }
            break;

        case E_PAGEUP:
        case E_PAGEDOWN:
            if (pi >= 0) {
                int dir = (id == E_PAGEUP) ? -1 : 1;

                dash_page_move(g_d, pi, dir);
                fill_pages();
                set(g_pages, MUIA_NList_Active, (LONG)(pi + dir));
                *changed = TRUE;
            }
            break;

        case E_GROUPNEW:
            if (pi >= 0 && page_add_group(&g_d->p[pi], GetStr(MSG_ED_NEWGROUP))) {
                fill_rows();
                *changed = TRUE;
            }
            break;

        case E_ADD:
            /* Leeres Suchfeld beim Oeffnen - ohne Benachrichtigung, sonst
             * wuerde die Liste gleich zweimal gefuellt. */
            SetAttrs(g_pick_find, MUIA_NoNotify, TRUE,
                     MUIA_String_Contents, (ULONG)"", TAG_DONE);
            fill_picker();
            set(g_pick_win, MUIA_Window_Open, TRUE);
            break;

        case E_PICKADD:
            if (picker_add()) {
                *changed = TRUE;
            }
            break;

        case E_PICKFIND:
            fill_picker();
            break;

        case E_PICKCLOSE:
            set(g_pick_win, MUIA_Window_Open, FALSE);
            break;

        case E_DEL:
            if (pi >= 0 && r >= 0) {
                if (g_row[r].wi < 0) {
                    page_group_remove(&g_d->p[pi], g_row[r].gi);
                } else {
                    group_widget_remove(&g_d->p[pi].g[g_row[r].gi],
                                        g_row[r].wi);
                }
                fill_rows();
                *changed = TRUE;
            }
            break;

        case E_UP:
        case E_DOWN:
            if (pi >= 0 && r >= 0) {
                int dir = (id == E_UP) ? -1 : 1;

                if (g_row[r].wi < 0) {
                    page_group_move(&g_d->p[pi], g_row[r].gi, dir);
                } else {
                    move_widget(pi, g_row[r].gi, g_row[r].wi, dir);
                }
                fill_rows();
                *changed = TRUE;
            }
            break;

        case E_RENAME: {
            char *txt = NULL;

            get(g_name, MUIA_String_Contents, &txt);
            if (!txt || !*txt || pi < 0) {
                break;
            }
            if (r < 0) {
                dash_set_title(g_d->p[pi].title, txt);
                fill_pages();
                set(g_pages, MUIA_NList_Active, (LONG)pi);
            } else if (g_row[r].wi < 0) {
                dash_set_title(g_d->p[pi].g[g_row[r].gi].title, txt);
                fill_rows();
            } else {
                dash_set_title(
                    g_d->p[pi].g[g_row[r].gi].w[g_row[r].wi].label, txt);
                fill_rows();
            }
            *changed = TRUE;
            break;
        }

        case E_ICON: {
            LONG n = 0;

            if (pi >= 0) {
                get(g_icon, MUIA_Cycle_Active, &n);
                g_d->p[pi].icon = (int)n;
                *changed = TRUE;
            }
            break;
        }

        case E_KIND: {
            LONG n = 0;

            if (pi >= 0 && r >= 0 && g_row[r].wi >= 0) {
                struct Widget *w =
                    &g_d->p[pi].g[g_row[r].gi].w[g_row[r].wi];

                get(g_kind, MUIA_Cycle_Active, &n);
                if (n >= WK_CHART) {
                    /* Diagramm: min ist der Zeitraum, max die Zahl der
                     * Balken. Neuer Zeitraum heisst neue Voreinstellung -
                     * 30 Tage oder 12 Monate. */
                    int period = (n == KIND_ENTRIES - 1) ? AH_PERIOD_MONTH
                                                         : AH_PERIOD_DAY;

                    if (w->kind != WK_CHART || w->min != period) {
                        w->min = period;
                        w->max = 0;
                    }
                    w->kind = WK_CHART;
                    dash_chart_defaults(w);
                } else {
                    /* Zurueck vom Diagramm: min/max hiessen dort etwas
                     * anderes, als Balkenbereich waeren sie Unsinn. */
                    if (w->kind == WK_CHART) {
                        w->min = 0;
                        w->max = 100;
                    }
                    w->kind = (int)n;
                    if (w->kind == WK_GAUGE && w->max <= w->min) {
                        w->max = w->min + 100;
                    }
                }
                fill_rows();
                *changed = TRUE;
            }
            break;
        }

        case E_PAGEDRAG:
            if (drag_pages()) {
                show_selection();
                *changed = TRUE;
            }
            break;

        case E_ROWDRAG: {
            int nr = -1;

            if (drag_rows(&nr)) {
                if (nr >= 0) {
                    set(g_rows, MUIA_NList_Active, (LONG)nr);
                }
                show_selection();
                *changed = TRUE;
            }
            break;
        }

        case E_SAVE:
            dash_save(g_d);
            *changed = TRUE;
            break;

        case E_CLOSE:
            set(g_pick_win, MUIA_Window_Open, FALSE);
            set(g_win, MUIA_Window_Open, FALSE);
            break;
    }
    return TRUE;
}

/* Die Seite, die im Editor gewaehlt ist, oder -1 bei geschlossenem Fenster.
 * Das Hauptfenster folgt ihr - siehe gui.c. */
int editor_page(void)
{
    LONG open = FALSE;

    if (!g_win) {
        return -1;
    }
    get(g_win, MUIA_Window_Open, &open);
    return open ? cur_page() : -1;
}

/* Das Hauptfenster hat die Dashboards veraendert (Uebernehmen in der
 * Geraeteauswahl, Aktualisieren). Ist der Editor offen, muessen seine
 * Listen neu entstehen - sonst zeigen sie auf Seiten, Kaesten und Zeilen,
 * die es so nicht mehr gibt, und der naechste Klick greift ins Leere. */
void editor_refresh(int page)
{
    LONG open = FALSE;

    if (!g_win) {
        return;
    }
    get(g_win, MUIA_Window_Open, &open);
    if (!open) {
        return;
    }
    fill_pages();
    if (g_d->count) {
        if (page < 0 || page >= g_d->count) {
            page = 0;
        }
        set(g_pages, MUIA_NList_Active, (LONG)page);
    }
    fill_rows();
    show_selection();
}
