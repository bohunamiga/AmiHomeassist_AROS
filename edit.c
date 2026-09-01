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
    E_SAVE, E_CLOSE,
    E_PICKADD, E_PICKCLOSE
};

static struct Dash    *g_d;
static struct Catalog *g_c;

static Object *g_win, *g_pages, *g_rows, *g_name, *g_icon, *g_kind;
static Object *g_iconview;
static Object *g_pick_win, *g_pick_list;

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
static const char *KIND_TEXT[WK_COUNT + 1];
static const char *ICON_TEXT[ICON_TEXT_COUNT + 1];

static const short KIND_MSG[WK_COUNT] = {
    MSG_KIND_TOGGLE, MSG_KIND_LAMP, MSG_KIND_VALUE,
    MSG_KIND_GAUGE,  MSG_KIND_COVER, MSG_KIND_TEXT
};

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
    KIND_TEXT[WK_COUNT] = NULL;      /* MUIO_Cycle liest bis zur NULL */

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
                    KIND_TEXT[g->w[k].kind]);
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
            set(g_kind, MUIA_Cycle_Active, (LONG)w->kind);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Geraeteauswahl                                                      */
/* ------------------------------------------------------------------ */

static void fill_picker(void)
{
    int i, n = 0;

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

    set(g_pick_list, MUIA_NList_Quiet, TRUE);
    DoMethod(g_pick_list, MUIM_NList_Clear);
    for (i = 0; i < g_c->count; i++) {
        if (entity_is_noise(&g_c->list[i])) {
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
                                MUIA_NList_Input, TRUE, TAG_DONE),
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
                                MUIA_NList_Input, TRUE, TAG_DONE),
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
            MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                MUIA_NListview_NList, g_pick_list = MUI_NewObject(MUIC_NList,
                    MUIA_NList_Input, TRUE, TAG_DONE),
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
    DoMethod(b_pclose, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, E_PICKCLOSE);

    return g_win;
}

void editor_open(void)
{
    if (!g_win) {
        return;                    /* Fenster kam nicht zustande */
    }
    fill_pages();
    if (g_d->count) {
        set(g_pages, MUIA_NList_Active, 0);
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
                fill_rows();
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
            fill_picker();
            set(g_pick_win, MUIA_Window_Open, TRUE);
            break;

        case E_PICKADD:
            if (picker_add()) {
                *changed = TRUE;
            }
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
                w->kind = (int)n;
                if (w->kind == WK_GAUGE && w->max <= w->min) {
                    w->max = w->min + 100;
                }
                fill_rows();
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
