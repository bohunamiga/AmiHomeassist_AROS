/* AmiHomeassist - MUI-Oberflaeche mit Seitenleiste.
 *
 * Links eine NList mit den Dashboards, rechts eine Gruppe im Seitenmodus,
 * dazwischen ein verschiebbarer Trenner - der Aufbau, den auch MUI
 * Preferences benutzt. Beim Umschalten wird nur MUIA_Group_ActivePage
 * gesetzt; die Seiten bleiben gebaut, statt jedes Mal neu zu entstehen. Auf
 * einem 68030 ist das der Unterschied zwischen "klickt" und "ruckelt".
 *
 * Anders als in einer Liste sind die Bedienelemente auf den Seiten echte
 * MUI-Objekte - Ankreuzfelder, Balken, Knoepfe. In Listenzeilen ginge das
 * nicht, dort werden nur Bilder gezeichnet; auf einer Seite darf man bauen.
 */

#include <exec/types.h>
#include <exec/lists.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <devices/timer.h>

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
#include "icons.h"
#include "edit.h"

extern struct DosLibrary *DOSBase;

struct IntuitionBase *IntuitionBase = NULL;
struct Library *MUIMasterBase = NULL;

const char *VERSTAG = "$VER: AmiHomeassist " AH_VERSION " (" AH_DATE ")";

enum {
    ID_REFRESH = 1, ID_PAGE,
    ID_OPEN_SEL, ID_SEL_CLICK, ID_SEL_ALL, ID_SEL_NONE, ID_SEL_SUGGEST,
    ID_SEL_APPLY,
    ID_OPEN_PREFS, ID_PREFS_SAVE, ID_OPEN_EDIT,
    ID_WIDGET = 1000            /* + laufende Nummer des Bedienelements */
};

static Object *app;
static Object *win, *lst_pages, *grp_pages, *txt_status;
static Object *win_sel, *lst_sel, *txt_sel;
static Object *win_prefs, *str_host, *str_token, *str_poll, *txt_prefs;
static Object *cyc_unknown;
static Object *img_side[MDI_COUNT];
static Object *img_sel[IMG_COUNT];

static struct Prefs    g_prefs;
static struct Catalog  g_cat;
static struct Dash     g_dash;
static BOOL            g_have_prefs = FALSE;

/* Die Workbench gibt einem Programm ohne Stack-Eintrag im Icon nur 4 KB.
 * MUI und NList brauchen allein mehr. Auf einem A500 mit TF536 lief der
 * Stapel darueber und zerstoerte fremden Speicher: erst Guru 8000 0003
 * (Sprung an eine ungerade Adresse), dann 0100 000F (Speicher doppelt
 * freigegeben). Auf dem PiStorm fiel es nie auf. libnix schaltet beim
 * Start auf diese Groesse um, gleich was im Icon oder in der Shell steht. */
unsigned long __stack = 65536;

/* Die Variable allein genuegt nicht: der Umschaltcode (swapstack.o in
 * libnix) kommt nur mit, wenn ihn etwas anfasst - sonst wird __stack
 * stillschweigend ignoriert. Dieser Zeiger zieht ihn herein. */
extern void __stkinit(void);
void (*const ah_force_stkswap)(void) = __stkinit;

/* Ein Bedienelement auf einer Seite, mit dem, was zum Nachfuehren noetig ist. */
struct WUI {
    struct Widget *w;
    struct Entity *e;
    Object        *ctl;         /* Gauge, Text oder die Schalter-Seitengruppe */
    Object        *img_off;     /* nur bei Schaltern: die beiden Bilder, */
    Object        *img_on;      /* damit beide den Klick melden koennen  */
    char           last[STATE_LEN];
    int            lastpos;     /* damit auch eine reine Fahrt auffaellt */
    int            lastcur;     /* nur Heizung: Ist und Soll, damit eine  */
    int            lasttgt;     /* Aenderung um ein halbes Grad auffaellt */
    Object        *ctl2;        /* nur Heizung: der Knopf mit der Betriebsart */
    Object        *row;         /* die ganze Zeile - zum Ausgrauen/Ausblenden */
    Object        *box;         /* der Kasten, in dem die Zeile steht */
    int            box_fixed;   /* Zeilen im Kasten ohne Geraet (Text, fehlt) */
    BOOL           dimmed;      /* was zuletzt gesetzt wurde - MUI nur bei */
    BOOL           hidden;      /* einer Aenderung anfassen, sonst flackert */
    BOOL           box_hidden;  /* nur im ersten Eintrag eines Kastens */
    char           info[STATE_LEN + UNIT_LEN + 8];  /* nur Balken, siehe unten */
};

static struct WUI *g_wui = NULL;
static int         g_wui_count = 0;
static int         g_wui_cap = 0;

/* Zeilentexte der Seitenleiste. Muessen gueltig bleiben, solange die
 * Eintraege stehen. */
#define SIDE_LEN (TITLE_LEN + 16)
static char *g_side = NULL;
static int   g_side_cap = 0;
#define SIDE(i) (g_side + (long)(i) * SIDE_LEN)

/* Puffer fuer die Auswahlliste. */
#define SELLABEL_LEN (AREA_LEN + NAME_LEN + 24)
static char *g_sel_label = NULL;
static char *g_sel_id = NULL;
static int   g_sel_cap = 0;
static int   g_sel_count = 0;
#define SELLABEL(i) (g_sel_label + (long)(i) * SELLABEL_LEN)
#define SELID(i)    (g_sel_id    + (long)(i) * ID_LEN)

/* ------------------------------------------------------------------ */
/* Kleinkram                                                           */
/* ------------------------------------------------------------------ */

static void say(Object *obj, const char *text)
{
    set(obj, MUIA_Text_Contents, (char *)text);
}

static void sayf(Object *obj, const char *fmt, long a, long b)
{
    static char buf[220];

    sprintf(buf, fmt, a, b);
    set(obj, MUIA_Text_Contents, buf);
}

static BOOL is_on(const struct Entity *e)
{
    return (BOOL)(stricmp(e->state, "on") == 0);
}

static BOOL is_lamp(const struct Entity *e)
{
    return (BOOL)(strncmp(e->id, "light.", 6) == 0);
}

/* binary_sensor liefert on/off - was das heisst, sagt erst die Geraeteklasse.
 * "Fenster: on" waere eine Zumutung. */
static const char *binary_text(const struct Entity *e)
{
    BOOL on = is_on(e);

    /* Nur "on" und "off" lassen sich uebersetzen. "unavailable" als "zu"
     * anzuzeigen hiesse, ein Fenster fuer geschlossen zu erklaeren, von dem
     * man gar nichts weiss. */
    if (stricmp(e->state, "on") != 0 && stricmp(e->state, "off") != 0) {
        return e->state;
    }

    if (strcmp(e->dclass, "window") == 0 || strcmp(e->dclass, "door") == 0 ||
        strcmp(e->dclass, "opening") == 0 ||
        strcmp(e->dclass, "garage_door") == 0) {
        return on ? GetStr(MSG_STATE_OPEN) : GetStr(MSG_STATE_CLOSED);
    }
    if (strcmp(e->dclass, "motion") == 0 ||
        strcmp(e->dclass, "occupancy") == 0) {
        return on ? GetStr(MSG_STATE_MOTION) : GetStr(MSG_STATE_QUIET);
    }
    if (strcmp(e->dclass, "moisture") == 0) {
        return on ? GetStr(MSG_STATE_WET) : GetStr(MSG_STATE_DRY);
    }
    return on ? GetStr(MSG_STATE_ON) : GetStr(MSG_STATE_OFF);
}

/* cover meldet open/closed/opening/closing. "offen" allein sagt bei einem
 * halb gefahrenen Rollladen wenig - deshalb die Stellung dazu, wenn der
 * Antrieb eine meldet. */
static const char *cover_text(const struct Entity *e)
{
    static char buf[32];
    const char *wort;

    if (stricmp(e->state, "open") == 0)         wort = GetStr(MSG_STATE_OPEN);
    else if (stricmp(e->state, "closed") == 0)  wort = GetStr(MSG_STATE_CLOSED);
    else if (stricmp(e->state, "opening") == 0) wort = GetStr(MSG_STATE_OPENING);
    else if (stricmp(e->state, "closing") == 0) wort = GetStr(MSG_STATE_CLOSING);
    else                                        wort = e->state;

    if (e->pos >= 0 && e->pos <= 100) {
        sprintf(buf, "%s %d %%", wort, e->pos);
        return buf;
    }
    return wort;
}

/* Die Betriebsart, wie Home Assistant sie nennt, in der Sprache des
 * Anwenders. Was nicht in der Liste steht, bleibt stehen wie es kam - HA
 * kennt Arten, die kein Amiga-Katalog vorhersehen kann. */
static const char *hvac_text(const char *mode)
{
    static const struct { const char *ha; short msg; } MAP[] = {
        { "off",       MSG_HVAC_OFF },
        { "heat",      MSG_HVAC_HEAT },
        { "cool",      MSG_HVAC_COOL },
        { "auto",      MSG_HVAC_AUTO },
        { "dry",       MSG_HVAC_DRY },
        { "fan_only",  MSG_HVAC_FAN },
        { "heat_cool", MSG_HVAC_HEATCOOL }
    };
    int i;

    for (i = 0; i < (int)(sizeof(MAP) / sizeof(MAP[0])); i++) {
        if (stricmp(mode, MAP[i].ha) == 0) {
            return GetStr(MAP[i].msg);
        }
    }
    return mode;
}

/* "23.5 -> 12.0 GradC". Die Einheit steht bei climate nicht in den
 * Attributen - Home Assistant rechnet alles in die Einheit der Anlage um und
 * sagt sie nicht dazu. Deshalb Grad Celsius, wenn nichts anderes dasteht. */
static const char *climate_text(const struct Entity *e)
{
    /* Grosszuegig: die Einheit kommt aus Home Assistant und darf bis
     * UNIT_LEN lang sein, zweimal, dazu zwei Temperaturen. */
    static char buf[96];
    char ist[16], soll[16];
    const char *unit = e->unit[0] ? e->unit : "\260C";

    temp_text(e->cur, ist, sizeof(ist));
    temp_text(e->tgt, soll, sizeof(soll));

    if (ist[0] && soll[0]) {
        sprintf(buf, "%s %s  >  %s %s", ist, unit, soll, unit);
    } else if (soll[0]) {
        sprintf(buf, ">  %s %s", soll, unit);
    } else if (ist[0]) {
        sprintf(buf, "%s %s", ist, unit);
    } else {
        strcpy(buf, "-");
    }
    return buf;
}


static void value_text(const struct Entity *e, char *out, int outsize)
{
    if (e->unit[0]) {
        sprintf(out, "%s %s", e->state, e->unit);
    } else {
        strncpy(out, e->state, outsize - 1);
        out[outsize - 1] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Speicher fuer die Zeilen                                            */
/* ------------------------------------------------------------------ */

static BOOL side_ensure(int n)
{
    if (n <= g_side_cap) {
        return TRUE;
    }
    if (g_side) {
        free(g_side);
    }
    g_side = malloc((size_t)n * SIDE_LEN);
    g_side_cap = g_side ? n : 0;
    return (BOOL)(g_side != NULL);
}

static BOOL sel_ensure(int n)
{
    if (n <= g_sel_cap) {
        return TRUE;
    }
    if (g_sel_label) {
        free(g_sel_label);
    }
    if (g_sel_id) {
        free(g_sel_id);
    }
    g_sel_label = malloc((size_t)n * SELLABEL_LEN);
    g_sel_id    = malloc((size_t)n * ID_LEN);
    if (!g_sel_label || !g_sel_id) {
        g_sel_cap = 0;
        return FALSE;
    }
    g_sel_cap = n;
    return TRUE;
}

static BOOL wui_add(struct Widget *w, struct Entity *e, Object *ctl,
                    Object *img_off, Object *img_on)
{
    if (g_wui_count >= g_wui_cap) {
        int nc = g_wui_cap ? g_wui_cap * 2 : 64;
        struct WUI *nw = (struct WUI *)
            realloc(g_wui, (size_t)nc * sizeof(struct WUI));
        if (!nw) {
            return FALSE;
        }
        g_wui = nw;
        g_wui_cap = nc;
    }
    memset(&g_wui[g_wui_count], 0, sizeof(struct WUI));
    g_wui[g_wui_count].lastpos = -2;      /* -1 ist ein gueltiger Wert */
    g_wui[g_wui_count].lastcur = TEMP_NONE - 1;
    g_wui[g_wui_count].lasttgt = TEMP_NONE - 1;
    g_wui[g_wui_count].w = w;
    g_wui[g_wui_count].e = e;
    g_wui[g_wui_count].img_off = img_off;
    g_wui[g_wui_count].img_on = img_on;
    g_wui[g_wui_count].ctl = ctl;
    g_wui_count++;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Seiten bauen                                                        */
/* ------------------------------------------------------------------ */

/* Die Bildbauer stehen weiter unten bei den Bildern, gebraucht werden sie
 * schon hier. */
static Object *make_image_ex(const struct IconDef *def, const ULONG *colors,
                             BOOL clickable);
static Object *make_image(const struct IconDef *def, const ULONG *colors);
static void unknown_apply(BOOL in_change);

/* Der Schalter: zwei gezeichnete Bilder in einer Seitengruppe. Umschalten
 * heisst dann nur, die sichtbare Seite zu wechseln - kein Austauschen von
 * Bilddaten zur Laufzeit, was bei Bodychunk heikel waere. Beide Bilder sind
 * anklickbar und melden dieselbe Nummer zurueck. */
static Object *switch_obj(BOOL on, Object **img_off, Object **img_on)
{
    Object *off = make_image_ex(&icon_switch_off, icon_colors, TRUE);
    Object *onn = make_image_ex(&icon_switch_on, icon_colors, TRUE);
    Object *grp;

    if (!off || !onn) {
        return NULL;
    }
    grp = MUI_NewObject(MUIC_Group,
        MUIA_Group_PageMode,   TRUE,
        MUIA_Group_ActivePage, on ? 1 : 0,
        MUIA_FixWidth,         icon_switch_off.width,
        MUIA_FixHeight,        icon_switch_off.height,
        MUIA_Group_Child,      off,
        MUIA_Group_Child,      onn,
        TAG_DONE);
    if (grp) {
        *img_off = off;
        *img_on  = onn;
    }
    return grp;
}

/* ------------------------------------------------------------------ */
/* Spalten                                                             */
/* ------------------------------------------------------------------ */

/* Eine Seite liest sich wie ein MUI-Formular: links die Beschriftung,
 * rechtsbuendig und mit Doppelpunkt, rechts der Wert, dahinter die Knoepfe.
 * Damit die Spalten ueber alle Kaesten einer Seite hinweg fluchten, bekommt
 * jede Beschriftung dieselbe feste Breite - die der laengsten auf der Seite -
 * und jeder Knopf dieselbe Breite wie der breiteste Knopftext.
 *
 * MUIA_FixWidthTxt merkt sich nur den Zeiger und misst bei jedem Layout neu.
 * Die Mustertexte muessen deshalb so lange leben wie die Objekte: je Seite
 * einer in g_lblfix, fuer die Knoepfe einer in g_btnfix. */
#define LBLFIX_LEN (TITLE_LEN + 4)
static char *g_lblfix = NULL;
static int   g_lblfix_cap = 0;
#define LBLFIX(i) (g_lblfix + (long)(i) * LBLFIX_LEN)

static char        g_btnfix[STATE_LEN + 4];
static const char *g_curlbl = "";   /* Muster der Seite, die gerade entsteht */

/* Knoepfe je Zeile bei Rollladen und Heizung - fuer den Platzhalter in
 * Zeilen ohne Knoepfe, damit die Wertspalte im Kasten buendig endet. */
#define ROW_BUTTONS 3

static BOOL lblfix_ensure(int n)
{
    if (n <= g_lblfix_cap) {
        return TRUE;
    }
    if (g_lblfix) {
        free(g_lblfix);
    }
    g_lblfix = malloc((size_t)n * LBLFIX_LEN);
    g_lblfix_cap = g_lblfix ? n : 0;
    return (BOOL)(g_lblfix != NULL);
}

/* Laengste Beschriftung der Seite, nach Zeichen gezaehlt. Das "M" davor ist
 * Luft fuer Proportionalschriften, in denen ein kuerzerer Text breiter sein
 * kann - bei rechtsbuendigen Beschriftungen landet es als Rand links. */
static void lblfix_for_page(const struct Page *p, char *out)
{
    size_t best = 0;
    int j, k;

    strcpy(out, "M:");
    for (j = 0; j < p->count; j++) {
        for (k = 0; k < p->g[j].count; k++) {
            const struct Widget *w = &p->g[j].w[k];
            size_t n = strlen(w->label);

            if (w->kind != WK_TEXT && n > best) {
                best = n;
                sprintf(out, "M%s:", w->label);
            }
        }
    }
}

/* Breitester Knopftext: Auf, Stop, Zu und alle Betriebsarten der Heizung.
 * Der Knopf der Betriebsart wechselt seinen Text zur Laufzeit - mit fester
 * Breite springt dabei nichts. */
static void btnfix_init(void)
{
    static const short MSG[] = {
        MSG_COVER_UP, MSG_COVER_STOP, MSG_COVER_DOWN,
        MSG_HVAC_OFF, MSG_HVAC_HEAT, MSG_HVAC_COOL, MSG_HVAC_AUTO,
        MSG_HVAC_DRY, MSG_HVAC_FAN, MSG_HVAC_HEATCOOL
    };
    int i;

    g_btnfix[0] = '\0';
    for (i = 0; i < (int)(sizeof(MSG) / sizeof(MSG[0])); i++) {
        const char *s = GetStr(MSG[i]);

        if (strlen(s) > strlen(g_btnfix) && strlen(s) < sizeof(g_btnfix)) {
            strcpy(g_btnfix, s);
        }
    }
}

/* Die Beschriftung. Der unsichtbare Rahmen (FramePhantomHoriz) gibt ihr
 * dieselbe Hoehe und Grundlinie wie dem umrahmten Wert daneben. */
static Object *label_obj(const char *text)
{
    char buf[TITLE_LEN + 2];

    sprintf(buf, "%s:", text);
    return MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents,     buf,
        MUIA_Text_PreParse,     "\33r",
        MUIA_Frame,             MUIV_Frame_Text,
        MUIA_FramePhantomHoriz, TRUE,
        MUIA_FixWidthTxt,       (char *)g_curlbl,
        TAG_DONE);
}

/* Ein Knopf mit fester Breite - wie MUIO_Button, nur dass dort keine
 * Breite vorzugeben ist. */
static Object *fix_button(const char *text)
{
    return MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (char *)text,
        MUIA_Text_PreParse, "\33c",
        MUIA_Frame,         MUIV_Frame_Button,
        MUIA_Background,    MUII_ButtonBack,
        MUIA_Font,          MUIV_Font_Button,
        MUIA_InputMode,     MUIV_InputMode_RelVerify,
        MUIA_CycleChain,    1,
        MUIA_FixWidthTxt,   (char *)g_btnfix,
        TAG_DONE);
}

/* Unsichtbarer Ersatz fuer die Knopfreihe, gleich breit. */
static Object *button_pad(void)
{
    Object *grp = MUI_NewObject(MUIC_Group,
        MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle,
            MUIA_FixWidthTxt, (char *)g_btnfix, TAG_DONE),
        TAG_DONE);
    int i;

    for (i = 1; grp && i < ROW_BUTTONS; i++) {
        DoMethod(grp, OM_ADDMEMBER, MUI_NewObject(MUIC_Rectangle,
            MUIA_FixWidthTxt, (char *)g_btnfix, TAG_DONE));
    }
    return grp;
}

/* Die Knopfreihe als eigene Gruppe - so hat sie genau denselben Aufbau wie
 * button_pad(), und beides ist gleich breit. */
static Object *button_row(Object *a, Object *b, Object *c)
{
    return MUI_NewObject(MUIC_Group,
        MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, a,
        MUIA_Group_Child, b,
        MUIA_Group_Child, c,
        TAG_DONE);
}

static Object *build_widget(struct Widget *w, BOOL pad)
{
    struct Entity *e = w->id[0] ? catalog_find(&g_cat, w->id) : NULL;
    Object *ctl = NULL;
    Object *row;
    Object *img_off = NULL, *img_on = NULL;
    char buf[STATE_LEN + UNIT_LEN + 8];

    if (w->kind == WK_TEXT) {
        return MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents, w->label,
            MUIA_Text_PreParse, "\33b",
            TAG_DONE);
    }

    /* Steht die Entitaet nicht im Katalog, ist sie in Home Assistant
     * verschwunden. Nicht stillschweigend weglassen, sondern zeigen - sonst
     * sucht der Anwender vergeblich. */
    if (!e) {
        return MUI_NewObject(MUIC_Group,
            MUIA_Group_Horiz, TRUE,
            MUIA_Group_Child, label_obj(w->label),
            MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                MUIA_Text_Contents, (char *)GetStr(MSG_STATE_MISSING),
                MUIA_Frame,         MUIV_Frame_Text,
                TAG_DONE),
            pad ? MUIA_Group_Child : TAG_IGNORE, pad ? button_pad() : NULL,
            TAG_DONE);
    }

    switch (w->kind) {
        case WK_TOGGLE: {
            Object *sym;

            ctl = switch_obj(is_on(e), &img_off, &img_on);
            sym = make_image(is_lamp(e) ? &icon_lamp : &icon_plug,
                             icon_colors);
            /* Name, Schalter, Sinnbild - der Name steht in derselben
             * Spalte wie bei allen anderen Zeilen, der Rest links
             * angeschlagen. */
            row = MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, label_obj(w->label),
                MUIA_Group_Child, ctl,
                MUIA_Group_Child, sym,
                MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle, TAG_DONE),
                TAG_DONE);
            if (row) {
                wui_add(w, e, ctl, img_off, img_on);
            }
            return row;
        }

        case WK_GAUGE:
            ctl = MUI_NewObject(MUIC_Gauge,
                MUIA_Gauge_Horiz,    TRUE,
                MUIA_Gauge_Max,      (LONG)(w->max - w->min),
                MUIA_Gauge_Current,  0,
                MUIA_Gauge_InfoText, "",
                MUIA_Frame,          MUIV_Frame_Gauge,
                MUIA_FixHeight,      14,
                TAG_DONE);
            break;

        case WK_LAMP:
            ctl = MUI_NewObject(MUIC_Text,
                MUIA_Text_Contents, (char *)binary_text(e),
                MUIA_Text_PreParse, "\33r",
                MUIA_Frame,         MUIV_Frame_Text,
                TAG_DONE);
            break;

        case WK_COVER: {
            Object *b_up   = fix_button(GetStr(MSG_COVER_UP));
            Object *b_stop = fix_button(GetStr(MSG_COVER_STOP));
            Object *b_down = fix_button(GetStr(MSG_COVER_DOWN));

            /* Der Zustandstext zeigt offen/geschlossen/faehrt. */
            ctl = MUI_NewObject(MUIC_Text,
                MUIA_Text_Contents, (char *)cover_text(e),
                MUIA_Text_PreParse, "\33r",
                MUIA_Frame,         MUIV_Frame_Text,
                TAG_DONE);

            row = MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, label_obj(w->label),
                MUIA_Group_Child, ctl,
                MUIA_Group_Child, button_row(b_up, b_stop, b_down),
                TAG_DONE);
            if (row) {
                wui_add(w, e, ctl, NULL, NULL);
                /* Die drei Knoepfe melden dieselbe Nummer, unterschieden
                 * durch den Abstand: +0 auf, +1 stop, +2 zu. Deshalb gehen
                 * die Nummern der Bedienelemente in VierersprÃ¼ngen. */
                {
                    int n = (g_wui_count - 1) * 4;

                    DoMethod(b_up, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 1);
                    DoMethod(b_stop, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 2);
                    DoMethod(b_down, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 3);
                }
            }
            return row;
        }

        case WK_CLIMATE: {
            Object *b_down = fix_button("-");
            Object *b_up   = fix_button("+");
            Object *b_mode = fix_button(hvac_text(e->state));

            ctl = MUI_NewObject(MUIC_Text,
                MUIA_Text_Contents, (char *)climate_text(e),
                MUIA_Text_PreParse, "\33r",
                MUIA_Frame,         MUIV_Frame_Text,
                TAG_DONE);

            row = MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Child, label_obj(w->label),
                MUIA_Group_Child, ctl,
                MUIA_Group_Child, button_row(b_down, b_up, b_mode),
                TAG_DONE);
            if (row) {
                wui_add(w, e, ctl, NULL, NULL);
                g_wui[g_wui_count - 1].ctl2 = b_mode;
                /* Dieselben Vierersprünge wie beim Rollladen:
                 * +1 kaelter, +2 waermer, +3 Betriebsart. */
                {
                    int n = (g_wui_count - 1) * 4;

                    DoMethod(b_down, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 1);
                    DoMethod(b_up, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 2);
                    DoMethod(b_mode, MUIM_Notify, MUIA_Pressed, FALSE,
                             app, 2, MUIM_Application_ReturnID,
                             ID_WIDGET + n + 3);
                }
            }
            return row;
        }

        default:                       /* WK_VALUE */
            value_text(e, buf, sizeof(buf));
            ctl = MUI_NewObject(MUIC_Text,
                MUIA_Text_Contents, buf,
                MUIA_Text_PreParse, "\33r",
                MUIA_Frame,         MUIV_Frame_Text,
                TAG_DONE);
            break;
    }

    if (!ctl) {
        return NULL;
    }

    row = MUI_NewObject(MUIC_Group,
        MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, label_obj(w->label),
        MUIA_Group_Child, ctl,
        pad ? MUIA_Group_Child : TAG_IGNORE, pad ? button_pad() : NULL,
        TAG_DONE);

    if (row) {
        wui_add(w, e, ctl, NULL, NULL);
    }
    return row;
}

static Object *build_group(struct Group *g)
{
    Object *box;
    BOOL pad = FALSE;
    int k, first, fixed = 0;

    box = MUI_NewObject(MUIC_Group,
        MUIA_Frame,      MUIV_Frame_Group,
        MUIA_FrameTitle, g->title,
        MUIA_Background, MUII_GroupBack,
        TAG_DONE);
    if (!box) {
        return NULL;
    }

    /* Hat der Kasten Zeilen mit Knoepfen, bekommen die anderen Zeilen
     * einen gleich breiten Platzhalter - sonst ragen deren Werte ueber die
     * Knoepfe hinaus, und die Wertspalte franst rechts aus. */
    for (k = 0; k < g->count; k++) {
        if (g->w[k].kind == WK_COVER || g->w[k].kind == WK_CLIMATE) {
            pad = TRUE;
        }
    }

    first = g_wui_count;
    for (k = 0; k < g->count; k++) {
        int before = g_wui_count;
        Object *row = build_widget(&g->w[k], pad);

        if (row) {
            DoMethod(box, OM_ADDMEMBER, row);
            if (g_wui_count > before) {
                g_wui[g_wui_count - 1].row = row;
            } else {
                fixed++;            /* Text oder verschwundenes Geraet */
            }
        }
    }
    for (k = first; k < g_wui_count; k++) {
        g_wui[k].box = box;
        g_wui[k].box_fixed = fixed;
    }
    return box;
}

static Object *build_page(struct Page *p, char *lblfix)
{
    Object *page;
    int j;

    lblfix_for_page(p, lblfix);
    g_curlbl = lblfix;

    page = MUI_NewObject(MUIC_Group, TAG_DONE);
    if (!page) {
        return NULL;
    }
    for (j = 0; j < p->count; j++) {
        Object *box = build_group(&p->g[j]);

        if (box) {
            DoMethod(page, OM_ADDMEMBER, box);
        }
    }
    /* Der Fueller schiebt die Kaesten nach oben, statt sie zu strecken. */
    DoMethod(page, OM_ADDMEMBER, MUI_NewObject(MUIC_Rectangle, TAG_DONE));
    return page;
}

/* Raeumt die Seitengruppe leer. Erst alle Kinder einsammeln, dann entfernen -
 * waehrend man die Kinderliste durchlaeuft, darf man sie nicht aendern. */
static void pages_clear(void)
{
    struct MinList *cl = NULL;
    Object *child;
    Object *list[128];
    APTR cstate;
    int n = 0;

    /* MUIA_Group_ChildList liefert die Liste selbst; NextObject will aber
     * deren erstes Element. Gibt man ihm die Liste, laeuft er durch
     * Speicher, der ihn nichts angeht - das endete in einer Exception 8. */
    get(grp_pages, MUIA_Group_ChildList, &cl);
    if (!cl) {
        return;
    }
    cstate = (APTR)cl->mlh_Head;
    while ((child = NextObject(&cstate)) != NULL && n < 128) {
        list[n++] = child;
    }
    while (n-- > 0) {
        DoMethod(grp_pages, OM_REMMEMBER, list[n]);
        MUI_DisposeObject(list[n]);
    }
    g_wui_count = 0;
}

static void pages_build(void)
{
    int i;

    DoMethod(grp_pages, MUIM_Group_InitChange);
    pages_clear();

    /* Erst jetzt, nach dem Abraeumen: die alten Objekte zeigten noch auf
     * die alten Mustertexte. */
    if (g_btnfix[0] == '\0') {
        btnfix_init();
    }
    if (!lblfix_ensure(g_dash.count > 0 ? g_dash.count : 1)) {
        DoMethod(grp_pages, MUIM_Group_ExitChange);
        return;
    }

    if (g_dash.count == 0) {
        DoMethod(grp_pages, OM_ADDMEMBER, MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            GetStr(MSG_EMPTY_DASH),
            TAG_DONE));
    } else {
        for (i = 0; i < g_dash.count; i++) {
            Object *page = build_page(&g_dash.p[i], LBLFIX(i));

            if (page) {
                DoMethod(grp_pages, OM_ADDMEMBER, page);
            }
        }
    }
    unknown_apply(TRUE);
    DoMethod(grp_pages, MUIM_Group_ExitChange);

    /* Bedienelemente melden sich mit ihrer laufenden Nummer zurueck. */
    for (i = 0; i < g_wui_count; i++) {
        if (g_wui[i].w->kind == WK_TOGGLE) {
            if (g_wui[i].img_off) {
                DoMethod(g_wui[i].img_off, MUIM_Notify, MUIA_Pressed, FALSE,
                         app, 2, MUIM_Application_ReturnID, ID_WIDGET + i * 4);
            }
            if (g_wui[i].img_on) {
                DoMethod(g_wui[i].img_on, MUIM_Notify, MUIA_Pressed, FALSE,
                         app, 2, MUIM_Application_ReturnID, ID_WIDGET + i * 4);
            }
        }
    }
}

static void sidebar_fill(void)
{
    int i;

    if (!side_ensure(g_dash.count > 0 ? g_dash.count : 1)) {
        return;
    }

    set(lst_pages, MUIA_NList_Quiet, TRUE);
    DoMethod(lst_pages, MUIM_NList_Clear);
    for (i = 0; i < g_dash.count; i++) {
        sprintf(SIDE(i), "\33o[%ld] %s", (long)g_dash.p[i].icon,
                g_dash.p[i].title);
        DoMethod(lst_pages, MUIM_NList_InsertSingle, SIDE(i),
                 MUIV_NList_Insert_Bottom);
    }
    set(lst_pages, MUIA_NList_Quiet, FALSE);
    if (g_dash.count) {
        set(lst_pages, MUIA_NList_Active, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Nachfuehren                                                         */
/* ------------------------------------------------------------------ */

static void widgets_update(void)
{
    int i;

    for (i = 0; i < g_wui_count; i++) {
        struct WUI *u = &g_wui[i];
        char buf[STATE_LEN + UNIT_LEN + 8];

        if (!u->e || !u->ctl) {
            continue;
        }
        if (strcmp(u->last, u->e->state) == 0 && u->lastpos == u->e->pos &&
                u->lastcur == u->e->cur && u->lasttgt == u->e->tgt) {
            continue;              /* unveraendert - nicht anfassen */
        }
        strcpy(u->last, u->e->state);
        u->lastpos = u->e->pos;
        u->lastcur = u->e->cur;
        u->lasttgt = u->e->tgt;

        switch (u->w->kind) {
            case WK_TOGGLE:
                set(u->ctl, MUIA_Group_ActivePage, is_on(u->e) ? 1 : 0);
                break;
            case WK_LAMP:
                set(u->ctl, MUIA_Text_Contents, (char *)binary_text(u->e));
                break;
            case WK_GAUGE: {
                long v = atol(u->e->state) - u->w->min;

                if (v < 0) {
                    v = 0;
                }
                /* Gauge kopiert MUIA_Gauge_InfoText NICHT, sondern merkt
                 * sich den Zeiger und liest ihn bei jedem Neuzeichnen neu.
                 * Ein Puffer auf dem Stapel ist dann laengst ueberschrieben:
                 * die Zahl stand nur, solange die Seite beim Aktualisieren
                 * sichtbar war, und verschwand beim ersten Seitenwechsel.
                 * Der Text muss deshalb so lange leben wie das Objekt. */
                value_text(u->e, u->info, sizeof(u->info));
                set(u->ctl, MUIA_Gauge_InfoText, u->info);
                set(u->ctl, MUIA_Gauge_Current, v);
                break;
            }
            case WK_VALUE:
                value_text(u->e, buf, sizeof(buf));
                set(u->ctl, MUIA_Text_Contents, buf);
                break;
            case WK_COVER:
                set(u->ctl, MUIA_Text_Contents, (char *)cover_text(u->e));
                break;
            case WK_CLIMATE:
                set(u->ctl, MUIA_Text_Contents, (char *)climate_text(u->e));
                if (u->ctl2) {
                    set(u->ctl2, MUIA_Text_Contents,
                        (char *)hvac_text(u->e->state));
                }
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Unbekannte Werte                                                    */
/* ------------------------------------------------------------------ */

/* Home Assistant meldet "unknown", wenn ein Sensor noch nie etwas geliefert
 * hat, und "unavailable", wenn das Geraet weg ist. Manche Integrationen
 * schreiben auch "None" oder gar nichts. */
static BOOL state_unknown(const struct Entity *e)
{
    const char *s = e->state;

    return (BOOL)(s[0] == '\0' || stricmp(s, "unknown") == 0 ||
                  stricmp(s, "unavailable") == 0 ||
                  stricmp(s, "none") == 0);
}

/* Graut aus oder blendet aus, je nach Einstellung. Gesetzt wird nur, was
 * sich geaendert hat: MUIA_ShowMe loest jedes Mal ein neues Layout des
 * ganzen Fensters aus, und das kostet auf einem 68030 sichtbar Zeit.
 *
 * Sind alle Zeilen eines Kastens ausgeblendet, verschwindet auch der Kasten -
 * ein leerer Rahmen mit Ueberschrift saehe aus wie ein Fehler. Zeilen ohne
 * Geraet (Beschriftungen, verschwundene Geraete) halten ihn sichtbar.
 *
 * MUIA_ShowMe allein genuegt unter MUI 3.8 nicht: auf der sichtbaren Seite
 * blieb danach ein zusammengefallenes Layout stehen, bis man die Seite
 * wechselte. Deshalb wird jede Aenderung der Sichtbarkeit in
 * InitChange/ExitChange geklammert - ausser der Aufrufer steht schon darin
 * (in_change, beim Neuaufbau). */
static BOOL row_hide(const struct WUI *u)
{
    return (BOOL)(u->e && state_unknown(u->e) &&
                  g_prefs.unknown == AH_UNK_HIDE);
}

static void unknown_apply(BOOL in_change)
{
    int i = 0;
    BOOL change = FALSE;

    if (!in_change) {
        for (i = 0; i < g_wui_count; i++) {
            if (g_wui[i].row && row_hide(&g_wui[i]) != g_wui[i].hidden) {
                change = TRUE;
                break;
            }
        }
        if (change) {
            DoMethod(grp_pages, MUIM_Group_InitChange);
        }
        i = 0;
    }

    while (i < g_wui_count) {
        Object *box = g_wui[i].box;
        int start = i;
        BOOL any = (BOOL)(g_wui[i].box_fixed > 0);

        for (; i < g_wui_count && g_wui[i].box == box; i++) {
            struct WUI *u = &g_wui[i];
            BOOL unk  = (BOOL)(u->e && state_unknown(u->e));
            BOOL dim  = (BOOL)(unk && g_prefs.unknown == AH_UNK_DIM);
            BOOL hide = row_hide(u);

            if (u->row && dim != u->dimmed) {
                set(u->row, MUIA_Disabled, dim);
                u->dimmed = dim;
            }
            if (u->row && hide != u->hidden) {
                set(u->row, MUIA_ShowMe, !hide);
                u->hidden = hide;
            }
            if (!u->hidden) {
                any = TRUE;
            }
        }
        if (box && (BOOL)!any != g_wui[start].box_hidden) {
            set(box, MUIA_ShowMe, any);
            g_wui[start].box_hidden = (BOOL)!any;
        }
    }
    if (change) {
        DoMethod(grp_pages, MUIM_Group_ExitChange);
    }
}

/* Fehlschlaege in Folge. Ist Home Assistant nicht erreichbar, hat es keinen
 * Sinn, weiter im eingestellten Takt dagegen zu laufen: jeder Versuch kostet
 * jetzt bis zu fuenf Sekunden Zeitgrenze, in denen die Oberflaeche steht.
 * Nach dem zweiten Fehlschlag wird der Abstand deshalb auf eine halbe Minute
 * gestreckt, der erste Erfolg stellt den eingestellten Takt wieder her. */
static int g_net_fails = 0;

#define NET_BACKOFF_AFTER 2
#define NET_BACKOFF_SECS 30

static int poll_secs(void)
{
    if (g_net_fails >= NET_BACKOFF_AFTER && g_prefs.poll < NET_BACKOFF_SECS) {
        return NET_BACKOFF_SECS;
    }
    return g_prefs.poll;
}

static void refresh_states(void)
{
    if (states_refresh(&g_prefs, &g_cat) != AH_OK) {
        say(txt_status, ha_last_error());
        if (g_net_fails < NET_BACKOFF_AFTER) {
            g_net_fails++;
        }
        return;
    }
    g_net_fails = 0;
    widgets_update();
    unknown_apply(FALSE);
}

static void status_summary(void)
{
    sayf(txt_status, GetStr(MSG_STATUS_DEVICES),
         (long)catalog_selected_count(&g_cat), (long)g_dash.count);
}

/* ------------------------------------------------------------------ */
/* Auswahl                                                             */
/* ------------------------------------------------------------------ */

static void selection_remember(void)
{
    int i;

    if (g_cat.count <= 0 || !sel_ensure(g_cat.count)) {
        return;
    }
    g_sel_count = 0;
    for (i = 0; i < g_cat.count; i++) {
        if (g_cat.list[i].selected) {
            strcpy(SELID(g_sel_count++), g_cat.list[i].id);
        }
    }
}

static void selection_restore(void)
{
    int i, j;

    for (i = 0; i < g_cat.count; i++) {
        g_cat.list[i].selected = FALSE;
        for (j = 0; j < g_sel_count; j++) {
            if (stricmp(g_cat.list[i].id, SELID(j)) == 0) {
                g_cat.list[i].selected = TRUE;
                break;
            }
        }
    }
}

static void sel_count_show(void)
{
    sayf(txt_sel, GetStr(MSG_STATUS_SELECTED),
         (long)catalog_selected_count(&g_cat), (long)g_cat.count);
}

static void sel_fill(void)
{
    int i;

    if (!sel_ensure(g_cat.count)) {
        say(txt_sel, GetStr(MSG_ERR_NOMEM));
        return;
    }

    set(lst_sel, MUIA_NList_Quiet, TRUE);
    DoMethod(lst_sel, MUIM_NList_Clear);
    for (i = 0; i < g_cat.count; i++) {
        struct Entity *e = &g_cat.list[i];

        sprintf(SELLABEL(i), "\33o[%ld] %-14s %s",
                (long)(e->selected ? IMG_CHECK_ON : IMG_CHECK_OFF),
                e->area, e->name);
        DoMethod(lst_sel, MUIM_NList_InsertSingle, SELLABEL(i),
                 MUIV_NList_Insert_Bottom);
    }
    set(lst_sel, MUIA_NList_Quiet, FALSE);
    sel_count_show();
}

static void sel_set_all(int mode)
{
    int i;

    for (i = 0; i < g_cat.count; i++) {
        if (mode == 0) {
            g_cat.list[i].selected = FALSE;
        } else if (mode == 1) {
            g_cat.list[i].selected = TRUE;
        } else {
            g_cat.list[i].selected = (BOOL)!entity_is_noise(&g_cat.list[i]);
        }
    }
    sel_fill();
}

static void sel_click(void)
{
    LONG pos = -1;

    get(lst_sel, MUIA_NList_Active, &pos);
    if (pos < 0 || pos >= g_cat.count) {
        return;
    }
    g_cat.list[pos].selected = (BOOL)!g_cat.list[pos].selected;
    SELLABEL(pos)[3] =
        (char)('0' + (g_cat.list[pos].selected ? IMG_CHECK_ON
                                               : IMG_CHECK_OFF));
    DoMethod(lst_sel, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
    sel_count_show();
}

/* ------------------------------------------------------------------ */
/* Einstellungen                                                       */
/* ------------------------------------------------------------------ */

static void prefs_to_gui(void)
{
    char buf[220];

    sprintf(buf, "http://%s:%d", g_prefs.host, g_prefs.port);
    set(str_host, MUIA_String_Contents, buf);
    set(str_token, MUIA_String_Contents, g_prefs.token);
    sprintf(buf, "%d", g_prefs.poll);
    set(str_poll, MUIA_String_Contents, buf);
    set(cyc_unknown, MUIA_Cycle_Active, (LONG)g_prefs.unknown);
}

static void prefs_from_gui(void)
{
    char *host = NULL, *token = NULL, *poll = NULL;
    char tmp[220];
    char *p;

    get(str_host, MUIA_String_Contents, &host);
    get(str_token, MUIA_String_Contents, &token);
    get(str_poll, MUIA_String_Contents, &poll);

    if (host) {
        strncpy(tmp, host, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        if (strnicmp(tmp, "http://", 7) == 0) {
            memmove(tmp, tmp + 7, strlen(tmp + 7) + 1);
        }
        p = strchr(tmp, '/');
        if (p) {
            *p = '\0';
        }
        g_prefs.port = 8123;
        p = strchr(tmp, ':');
        if (p) {
            *p = '\0';
            g_prefs.port = atoi(p + 1);
            if (g_prefs.port <= 0) {
                g_prefs.port = 8123;
            }
        }
        strncpy(g_prefs.host, tmp, sizeof(g_prefs.host) - 1);
        g_prefs.host[sizeof(g_prefs.host) - 1] = '\0';
    }
    if (token) {
        strncpy(g_prefs.token, token, sizeof(g_prefs.token) - 1);
        g_prefs.token[sizeof(g_prefs.token) - 1] = '\0';
    }
    if (poll) {
        g_prefs.poll = atoi(poll);
    }
    if (g_prefs.poll < 2) {
        g_prefs.poll = 2;
    }
    {
        LONG unk = AH_UNK_DIM;

        get(cyc_unknown, MUIA_Cycle_Active, &unk);
        g_prefs.unknown = (int)unk;
    }
}

/* ------------------------------------------------------------------ */
/* Bilder                                                              */
/* ------------------------------------------------------------------ */

/* Ein Bild als Bedienelement. MUIA_InputMode gehoert der Area-Klasse, also
 * kann jedes Bild anklickbar sein - man braucht dafuer keine eigene Klasse
 * und keinen Knopf drumherum. */
static Object *make_image_ex(const struct IconDef *def, const ULONG *colors,
                             BOOL clickable)
{
    return MUI_NewObject(MUIC_Bodychunk,
        MUIA_InputMode,   clickable ? MUIV_InputMode_RelVerify
                                    : MUIV_InputMode_None,
        MUIA_ShowSelState, FALSE,
        MUIA_FixWidth,              def->width,
        MUIA_FixHeight,             def->height,
        MUIA_Bitmap_Width,          def->width,
        MUIA_Bitmap_Height,         def->height,
        MUIA_Bodychunk_Depth,       def->depth,
        MUIA_Bodychunk_Body,        (UBYTE *)def->body,
        MUIA_Bodychunk_Compression, 0,
        MUIA_Bodychunk_Masking,     0,
        MUIA_Bitmap_SourceColors,   (ULONG *)colors,
        MUIA_Bitmap_Transparent,    0,
        TAG_DONE);
}

static Object *make_image(const struct IconDef *def, const ULONG *colors)
{
    return make_image_ex(def, colors, FALSE);
}

/* Jede Liste braucht eigene Bildobjekte: laut Autodoc gehoert ein Bildobjekt
 * genau einer NList, weil es ihr als Member zugeordnet wird. */
static void register_side_images(void)
{
    int i;

    for (i = 0; i < MDI_COUNT; i++) {
        img_side[i] = make_image(mdi_icon(i), mdi_colors);
        if (img_side[i]) {
            DoMethod(lst_pages, MUIM_NList_UseImage, img_side[i], (ULONG)i, 0);
        }
    }
}

static void register_sel_images(void)
{
    const struct IconDef *defs[IMG_COUNT];
    int i;

    defs[IMG_SWITCH_OFF] = &icon_switch_off;
    defs[IMG_SWITCH_ON]  = &icon_switch_on;
    defs[IMG_LAMP]       = &icon_lamp;
    defs[IMG_PLUG]       = &icon_plug;
    defs[IMG_CHECK_OFF]  = &icon_check_off;
    defs[IMG_CHECK_ON]   = &icon_check_on;

    for (i = 0; i < IMG_COUNT; i++) {
        img_sel[i] = make_image(defs[i], icon_colors);
        if (img_sel[i]) {
            DoMethod(lst_sel, MUIM_NList_UseImage, img_sel[i], (ULONG)i, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Zeitgeber                                                           */
/* ------------------------------------------------------------------ */

static struct MsgPort     *g_tport = NULL;
static struct timerequest *g_treq  = NULL;
static ULONG               g_tsig  = 0;
static BOOL                g_twait = FALSE;

static BOOL timer_open(void)
{
    g_tport = CreateMsgPort();
    if (!g_tport) {
        return FALSE;
    }
    g_treq = (struct timerequest *)
                 CreateIORequest(g_tport, sizeof(struct timerequest));
    if (!g_treq) {
        DeleteMsgPort(g_tport);
        g_tport = NULL;
        return FALSE;
    }
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK,
                   (struct IORequest *)g_treq, 0) != 0) {
        DeleteIORequest((struct IORequest *)g_treq);
        DeleteMsgPort(g_tport);
        g_treq = NULL;
        g_tport = NULL;
        return FALSE;
    }
    g_tsig = 1UL << g_tport->mp_SigBit;
    return TRUE;
}

static void timer_start(int secs)
{
    if (!g_treq || g_twait) {
        return;
    }
    g_treq->tr_node.io_Command = TR_ADDREQUEST;
    g_treq->tr_time.tv_secs    = secs;
    g_treq->tr_time.tv_micro   = 0;
    SendIO((struct IORequest *)g_treq);
    g_twait = TRUE;
}

static void timer_stop(void)
{
    if (g_treq && g_twait) {
        AbortIO((struct IORequest *)g_treq);
        WaitIO((struct IORequest *)g_treq);
        g_twait = FALSE;
    }
}

static void timer_close(void)
{
    timer_stop();
    if (g_treq) {
        CloseDevice((struct IORequest *)g_treq);
        DeleteIORequest((struct IORequest *)g_treq);
        g_treq = NULL;
    }
    if (g_tport) {
        DeleteMsgPort(g_tport);
        g_tport = NULL;
    }
}

/* ------------------------------------------------------------------ */

/* Die Beschriftungen im Einstellungsfenster, rechtsbuendig (das macht
 * MUIO_Label von selbst) und mit Doppelpunkt. Der steht nicht in den
 * Katalogen, damit sich an den Uebersetzungen nichts aendert. MUIO_Label
 * kopiert den Text, der Puffer darf also auf dem Stapel liegen. */
static Object *colon_label(int msg)
{
    char buf[80];

    sprintf(buf, "%.76s:", GetStr(msg));
    return MUI_MakeObject(MUIO_Label, buf, 0);
}

/* Muss statisch sein und mit NULL enden - siehe KIND_TEXT in edit.c. */
static const char *UNK_TEXT[4];

static Object *button(const char *label)
{
    return MUI_MakeObject(MUIO_Button, (char *)label);
}

/* Holt Katalog und Dashboards und baut die Anzeige neu auf. */
static void reload_all(BOOL fetch)
{
    int found = 0;

    if (fetch) {
        say(txt_status, GetStr(MSG_STATUS_QUERYING));
        selection_remember();
        {
            int before = g_cat.count;

            if (catalog_fetch(&g_prefs, &g_cat) != AH_OK) {
                g_cat.count = before;
                say(txt_status, ha_last_error());
                return;
            }
        }
        if (g_sel_count) {
            selection_restore();
        } else {
            import_load(&g_cat, &found);
        }
        catalog_sort(&g_cat);
    }

    if (dash_load(&g_dash) != AH_OK || g_dash.count == 0) {
        /* Noch keine Dashboards: aus der Auswahl je Raum eine Seite. */
        dash_generate(&g_dash, &g_cat);
        if (g_dash.count) {
            dash_save(&g_dash);
        }
    }
    dash_mark_used(&g_dash, &g_cat);

    sidebar_fill();
    pages_build();
    status_summary();
}

int main(void)
{
    ULONG sigs = 0;
    ULONG id;
    Object *bt_refresh, *bt_sel, *bt_prefs, *bt_edit;
    Object *bt_all, *bt_none, *bt_suggest, *bt_apply, *bt_save;

    /* Zuerst die Sprache - danach ist jede Meldung uebersetzt, auch die
     * beiden Fehlermeldungen gleich hier drunter. */
    locale_open();

    IntuitionBase = (struct IntuitionBase *)
                        OpenLibrary("intuition.library", 37);
    if (!IntuitionBase) {
        printf("%s\n", GetStr(MSG_ERR_NOINTUITION));
        locale_close();
        return 20;
    }
    MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
    if (!MUIMasterBase) {
        printf("%s\n", GetStr(MSG_ERR_NOMUI));
        CloseLibrary((struct Library *)IntuitionBase);
        locale_close();
        return 20;
    }

    catalog_init(&g_cat);
    dash_init(&g_dash);
    g_have_prefs = (BOOL)(prefs_load(&g_prefs) == AH_OK);

    UNK_TEXT[AH_UNK_SHOW] = GetStr(MSG_UNK_SHOW);
    UNK_TEXT[AH_UNK_DIM]  = GetStr(MSG_UNK_DIM);
    UNK_TEXT[AH_UNK_HIDE] = GetStr(MSG_UNK_HIDE);
    UNK_TEXT[3] = NULL;

    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title,       "AmiHomeassist",
        MUIA_Application_Version,     (char *)VERSTAG,
        MUIA_Application_Copyright,   "2026",
        MUIA_Application_Author,      "Radi",
        MUIA_Application_Description, (char *)GetStr(MSG_APP_DESCRIPTION),
        MUIA_Application_Base,        "AMIHOMEASSIST",

        MUIA_Application_Window, win = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title,  "AmiHomeassist",
            MUIA_Window_ID,     MAKE_ID('A','H','A','1'),
            MUIA_Window_Width,  MUIV_Window_Width_Visible(50),
            MUIA_Window_Height, MUIV_Window_Height_Visible(60),
            MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,

                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,

                    /* Die Seitenleiste bekommt oben das Logo. Ein echtes
                     * Hintergrundbild kann MUI nur aus seinen eigenen
                     * Mustern, deshalb ein eigener Kasten - der sitzt an
                     * derselben Stelle wie das Logo in Home Assistant. */
                    MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                        MUIA_HorizWeight, 30,

                        MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                            MUIA_Frame,      MUIV_Frame_Group,
                            MUIA_Background, MUII_GroupBack,
                            MUIA_VertWeight, 0,
                            MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                                MUIA_Group_Horiz, TRUE,
                                MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle,
                                    TAG_DONE),
                                MUIA_Group_Child,
                                    make_image(&logo_image, logo_colors),
                                MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle,
                                    TAG_DONE),
                                TAG_DONE),
                            /* \33b fett, \33P[1] in der Stiftfarbe fuer
                             * Text - auf dem Grau der Gruppe ist das
                             * Schwarz. */
                            MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                                MUIA_Text_Contents, "\33c\33b\33P[1]AmiHomeassist",
                                TAG_DONE),
                            MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                                MUIA_Text_Contents, "\33c\33P[1]v" AH_VERSION,
                                MUIA_Font,          MUIV_Font_Tiny,
                                TAG_DONE),
                            TAG_DONE),

                        MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                            MUIA_NListview_NList,
                                lst_pages = MUI_NewObject(MUIC_NList,
                                    MUIA_NList_Input, TRUE,
                                    TAG_DONE),
                            TAG_DONE),
                        TAG_DONE),

                    MUIA_Group_Child, MUI_NewObject(MUIC_Balance, TAG_DONE),

                    /* Seitenmodus: alle Seiten bleiben gebaut, sichtbar ist
                     * immer nur eine. Umschalten kostet dann nichts. */
                    MUIA_Group_Child, grp_pages = MUI_NewObject(MUIC_Group,
                        MUIA_Group_PageMode, TRUE,
                        MUIA_HorizWeight,    100,
                        MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle,
                            TAG_DONE),
                        TAG_DONE),
                    TAG_DONE),

                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, bt_refresh = button((char *)GetStr(MSG_BT_REFRESH)),
                    MUIA_Group_Child, bt_sel     = button((char *)GetStr(MSG_BT_SELECT)),
                    MUIA_Group_Child, bt_edit    = button((char *)GetStr(MSG_BT_EDIT)),
                    MUIA_Group_Child, bt_prefs   = button((char *)GetStr(MSG_BT_PREFS)),
                    TAG_DONE),

                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, txt_status = MUI_NewObject(MUIC_Text,
                        MUIA_Text_Contents, "",
                        MUIA_Frame,         MUIV_Frame_Text,
                        TAG_DONE),
                    TAG_DONE),
                TAG_DONE),
            TAG_DONE),

        MUIA_Application_Window, win_sel = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title,  (char *)GetStr(MSG_WIN_SELECT),
            MUIA_Window_ID,     MAKE_ID('A','H','A','2'),
            MUIA_Window_Width,  MUIV_Window_Width_Visible(45),
            MUIA_Window_Height, MUIV_Window_Height_Visible(65),
            MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,
                MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                    MUIA_Text_Contents,
                    (char *)GetStr(MSG_HINT_SELECT),
                    TAG_DONE),
                MUIA_Group_Child, MUI_NewObject(MUIC_NListview,
                    MUIA_NListview_NList, lst_sel = MUI_NewObject(MUIC_NList,
                        MUIA_NList_Input, TRUE,
                        TAG_DONE),
                    TAG_DONE),
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, bt_suggest = button((char *)GetStr(MSG_BT_SUGGEST)),
                    MUIA_Group_Child, bt_all     = button((char *)GetStr(MSG_BT_ALL)),
                    MUIA_Group_Child, bt_none    = button((char *)GetStr(MSG_BT_NONE)),
                    TAG_DONE),
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, txt_sel = MUI_NewObject(MUIC_Text,
                        MUIA_Text_Contents, "",
                        MUIA_Frame,         MUIV_Frame_Text,
                        TAG_DONE),
                    MUIA_Group_Child, bt_apply = button((char *)GetStr(MSG_BT_APPLY)),
                    TAG_DONE),
                TAG_DONE),
            TAG_DONE),

        MUIA_Application_Window, win_prefs = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title,  (char *)GetStr(MSG_WIN_PREFS),
            MUIA_Window_ID,     MAKE_ID('A','H','A','3'),
            MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Columns, 2,
                    MUIA_Group_Child, colon_label(MSG_LBL_ADDRESS),
                    MUIA_Group_Child, str_host = MUI_NewObject(MUIC_String,
                        MUIA_String_MaxLen, 160,
                        MUIA_Frame,         MUIV_Frame_String,
                        TAG_DONE),
                    MUIA_Group_Child, colon_label(MSG_LBL_TOKEN),
                    MUIA_Group_Child, str_token = MUI_NewObject(MUIC_String,
                        MUIA_String_MaxLen, 600,
                        MUIA_Frame,         MUIV_Frame_String,
                        TAG_DONE),
                    MUIA_Group_Child, colon_label(MSG_LBL_INTERVAL),
                    MUIA_Group_Child, str_poll = MUI_NewObject(MUIC_String,
                        MUIA_String_MaxLen, 8,
                        MUIA_String_Accept, "0123456789",
                        MUIA_Frame,         MUIV_Frame_String,
                        TAG_DONE),
                    MUIA_Group_Child, colon_label(MSG_LBL_UNKNOWN),
                    MUIA_Group_Child, cyc_unknown =
                        MUI_MakeObject(MUIO_Cycle, NULL, (char **)UNK_TEXT),
                    TAG_DONE),
                MUIA_Group_Child, txt_prefs = MUI_NewObject(MUIC_Text,
                    MUIA_Text_Contents,
                    (char *)GetStr(MSG_HINT_TOKEN),
                    MUIA_Frame, MUIV_Frame_Text,
                    TAG_DONE),
                MUIA_Group_Child, bt_save = button((char *)GetStr(MSG_BT_SAVE)),
                TAG_DONE),
            TAG_DONE),

        TAG_DONE);

    if (!app) {
        printf("%s\n", GetStr(MSG_ERR_NOGUI));
        CloseLibrary(MUIMasterBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_refresh, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_REFRESH);
    DoMethod(bt_sel, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_OPEN_SEL);
    DoMethod(bt_prefs, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_OPEN_PREFS);
    DoMethod(bt_edit, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_OPEN_EDIT);
    DoMethod(lst_pages, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, ID_PAGE);

    DoMethod(win_sel, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             win_sel, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(lst_sel, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime,
             app, 2, MUIM_Application_ReturnID, ID_SEL_CLICK);
    DoMethod(bt_all, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SEL_ALL);
    DoMethod(bt_none, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SEL_NONE);
    DoMethod(bt_suggest, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SEL_SUGGEST);
    DoMethod(bt_apply, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_SEL_APPLY);

    DoMethod(win_prefs, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             win_prefs, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(bt_save, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_PREFS_SAVE);

    register_side_images();
    register_sel_images();
    editor_build(app, &g_dash, &g_cat);

    prefs_to_gui();
    set(win, MUIA_Window_Open, TRUE);

    if (!g_have_prefs) {
        say(txt_status, ha_last_error());
        set(win_prefs, MUIA_Window_Open, TRUE);
    } else {
        reload_all(TRUE);
    }

    if (timer_open()) {
        timer_start(poll_secs());
    }

    while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs))
                != (ULONG)MUIV_Application_ReturnID_Quit) {

        {
            BOOL changed = FALSE;

            if (editor_handle(id, &changed)) {
                if (changed) {
                    /* Der Editor hat das Modell veraendert - Seitenleiste und
                     * Seiten muessen neu entstehen, und der Sparbetrieb muss
                     * wissen, was jetzt gebraucht wird. */
                    dash_mark_used(&g_dash, &g_cat);
                    sidebar_fill();
                    pages_build();
                    status_summary();
                }
                if (sigs) {
                    sigs = Wait(sigs | SIGBREAKF_CTRL_C | g_tsig);
                    if (sigs & g_tsig) {
                        while (GetMsg(g_tport)) {
                            ;
                        }
                        g_twait = FALSE;
                        timer_start(poll_secs());
                    }
                    if (sigs & SIGBREAKF_CTRL_C) {
                        break;
                    }
                }
                continue;
            }
        }

        if (id >= ID_WIDGET) {
            int raw = (int)(id - ID_WIDGET);
            int n   = raw / 4;          /* welches Bedienelement */
            int act = raw % 4;          /* 0 schalten, 1 auf, 2 stop, 3 zu */

            if (n >= 0 && n < g_wui_count && g_wui[n].e && act > 0 &&
                    g_wui[n].w->kind == WK_CLIMATE) {
                struct WUI *u = &g_wui[n];

                if (act == 3) {
                    char mode[STATE_LEN];

                    if (!hvac_next_mode(u->e, mode, sizeof(mode))) {
                        say(txt_status, u->w->label);
                    } else if (ha_set_hvac_mode(&g_prefs, u->e->id, mode)
                                   != AH_OK) {
                        say(txt_status, ha_last_error());
                    } else {
                        /* Sofort umschreiben, nicht auf die naechste Abfrage
                         * warten - ein Knopf, der erst in einer Sekunde
                         * reagiert, fuehlt sich kaputt an. */
                        strcpy(u->e->state, mode);
                        strcpy(u->last, u->e->state);
                        if (u->ctl2) {
                            set(u->ctl2, MUIA_Text_Contents,
                                (char *)hvac_text(u->e->state));
                        }
                        say(txt_status, u->w->label);
                    }
                } else if (u->e->tgt == TEMP_NONE) {
                    say(txt_status, u->w->label);
                } else {
                    int step = (u->e->tstep > 0) ? u->e->tstep : 5;
                    int t = u->e->tgt + ((act == 2) ? step : -step);

                    if (t < u->e->tmin) {
                        t = u->e->tmin;
                    }
                    if (t > u->e->tmax) {
                        t = u->e->tmax;
                    }
                    if (t == u->e->tgt) {
                        say(txt_status, u->w->label);   /* am Anschlag */
                    } else if (ha_set_temperature(&g_prefs, u->e->id, t)
                                   != AH_OK) {
                        say(txt_status, ha_last_error());
                    } else {
                        u->e->tgt = (short)t;
                        u->lasttgt = t;
                        set(u->ctl, MUIA_Text_Contents,
                            (char *)climate_text(u->e));
                        say(txt_status, u->w->label);
                    }
                }
            } else if (n >= 0 && n < g_wui_count && g_wui[n].e && act > 0) {
                struct WUI *u = &g_wui[n];
                const char *svc = (act == 1) ? "open_cover"
                                : (act == 2) ? "stop_cover" : "close_cover";

                if (ha_service(&g_prefs, u->e->id, svc) != AH_OK) {
                    say(txt_status, ha_last_error());
                } else {
                    say(txt_status, u->w->label);
                }
            } else if (n >= 0 && n < g_wui_count && g_wui[n].e) {
                struct WUI *u = &g_wui[n];

                if (ha_service(&g_prefs, u->e->id, "toggle") != AH_OK) {
                    say(txt_status, ha_last_error());
                } else {
                    strcpy(u->e->state, is_on(u->e) ? "off" : "on");
                    strcpy(u->last, u->e->state);
                    set(u->ctl, MUIA_Group_ActivePage,
                        is_on(u->e) ? 1 : 0);
                    say(txt_status, u->w->label);
                }
            }
        } else {
            switch (id) {
                case ID_REFRESH:
                    reload_all(TRUE);
                    break;

                case ID_PAGE: {
                    LONG n = 0;

                    get(lst_pages, MUIA_NList_Active, &n);
                    if (n >= 0 && n < g_dash.count) {
                        set(grp_pages, MUIA_Group_ActivePage, (LONG)n);
                    }
                    break;
                }

                case ID_OPEN_SEL:
                    sel_fill();
                    set(win_sel, MUIA_Window_Open, TRUE);
                    break;
                case ID_SEL_CLICK:   sel_click(); break;
                case ID_SEL_ALL:     sel_set_all(1); break;
                case ID_SEL_NONE:    sel_set_all(0); break;
                case ID_SEL_SUGGEST: sel_set_all(2); break;
                case ID_SEL_APPLY:
                    import_save(&g_cat);
                    selection_remember();
                    /* Aus der neuen Auswahl frische Seiten bauen. */
                    dash_generate(&g_dash, &g_cat);
                    dash_save(&g_dash);
                    dash_mark_used(&g_dash, &g_cat);
                    sidebar_fill();
                    pages_build();
                    status_summary();
                    set(win_sel, MUIA_Window_Open, FALSE);
                    break;

                case ID_OPEN_EDIT:
                    editor_open();
                    break;

                case ID_OPEN_PREFS:
                    prefs_to_gui();
                    set(win_prefs, MUIA_Window_Open, TRUE);
                    break;
                case ID_PREFS_SAVE:
                    prefs_from_gui();
                    if (prefs_save(&g_prefs) != AH_OK) {
                        say(txt_prefs, ha_last_error());
                    } else {
                        say(txt_prefs, GetStr(MSG_STATUS_SAVED));
                        g_have_prefs = TRUE;
                        set(win_prefs, MUIA_Window_Open, FALSE);
                        timer_stop();
                        reload_all(TRUE);
                        timer_start(poll_secs());
                    }
                    break;
            }
        }

        if (sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C | g_tsig);

            if (sigs & g_tsig) {
                LONG selopen = FALSE;

                while (GetMsg(g_tport)) {
                    ;
                }
                g_twait = FALSE;
                get(win_sel, MUIA_Window_Open, &selopen);
                if (g_have_prefs && !selopen) {
                    refresh_states();
                }
                timer_start(poll_secs());
            }
            if (sigs & SIGBREAKF_CTRL_C) {
                break;
            }
        }
    }

    timer_close();
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    dash_free(&g_dash);
    catalog_free(&g_cat);
    CloseLibrary(MUIMasterBase);
    CloseLibrary((struct Library *)IntuitionBase);
    locale_close();
    return 0;
}
