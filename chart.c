/* AmiHomeassist - Balkendiagramm als eigene MUI-Klasse. Siehe chart.h. */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/gfxmacros.h>   /* SetDrPt ist ein Makro */
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <intuition/screens.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <string.h>

#include "amiha.h"
#include "amiloc.h"
#include "chart.h"

extern struct Library *MUIMasterBase;

struct ChartData {
    struct StatPoint pt[CHART_MAX];
    int  n;
    int  period;
    char unit[UNIT_LEN];
    BOOL shown;                 /* zwischen MUIM_Show und MUIM_Hide */
};

static struct MUI_CustomClass *g_mcc = NULL;

/* Monatsnamen kommen als EIN Katalogtext, durch Leerzeichen getrennt -
 * zwoelf einzelne Texte waeren zwoelf Nummern mehr fuer jeden Uebersetzer. */
static void month_name(int m, char *out, int outsize)
{
    const char *s = GetStr(MSG_CHART_MONTHS);
    int i = 1, k = 0;

    while (*s && i < m) {
        if (*s++ == ' ') {
            i++;
        }
    }
    while (*s && *s != ' ' && k < outsize - 1) {
        out[k++] = *s++;
    }
    out[k] = '\0';
}

/* Hundertstel als Achsbeschriftung. Ist der Rasterschritt eine ganze Zahl,
 * genuegt die ganze Zahl - "400" liest sich besser als "400.00". */
static void value_label(long v, long step, char *out)
{
    long a = v < 0 ? -v : v;

    if (step % 100 == 0) {
        sprintf(out, "%s%ld", v < 0 ? "-" : "", a / 100);
    } else if (step % 10 == 0) {
        sprintf(out, "%s%ld.%ld", v < 0 ? "-" : "", a / 100, (a % 100) / 10);
    } else {
        sprintf(out, "%s%ld.%02ld", v < 0 ? "-" : "", a / 100, a % 100);
    }
}

/* Rasterschritt 1, 2 oder 5 mal einer Zehnerpotenz: der kleinste, bei dem
 * hoechstens 'lines' Linien entstehen. Mit 5 sieht es aus wie in Home
 * Assistant; bei wenig Hoehe werden es weniger, sonst stehen die Zahlen
 * an der Achse uebereinander. */
static long nice_step(long max, int lines)
{
    static const int MULT[3] = { 1, 2, 5 };
    long mag = 1;
    int k = 0;

    if (max <= 0) {
        return 100;
    }
    if (lines < 1) {
        lines = 1;
    }
    for (;;) {
        long step = mag * MULT[k];

        if (step * lines >= max) {
            return step;
        }
        if (++k == 3) {
            k = 0;
            mag *= 10;
        }
    }
}

static void draw_text(struct RastPort *rp, int x, int y, const char *s)
{
    Move(rp, x, y);
    Text(rp, (STRPTR)s, strlen(s));
}

static void chart_draw(struct IClass *cl, Object *obj)
{
    struct ChartData *d = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    struct TextFont *tf = _font(obj);
    UWORD *pens = _dri(obj)->dri_Pens;
    int l = _mleft(obj), t = _mtop(obj);
    int r = _mright(obj), b = _mbottom(obj);
    int fh = tf->tf_YSize, base = tf->tf_Baseline;
    long max = 0, step, top;
    int px0, px1, py0, py1, lines, i, lm = 0;
    char buf[32];

    SetFont(rp, tf);
    SetDrMd(rp, JAM1);

    /* Keine verwertbaren Werte? Leistungssensoren (W, kW) fuehren nur
     * Mittelwerte, keine "change" - Home Assistant liefert dann lauter
     * null. Eine leere Achse bis 1 saehe aus wie "nichts verbraucht". */
    {
        int k, valid = 0;

        for (k = 0; k < d->n; k++) {
            if (d->pt[k].valid) {
                valid++;
            }
        }
        if (valid == 0) {
            d->n = 0;
        }
    }

    if (d->n == 0) {
        const char *s = GetStr(MSG_CHART_EMPTY);
        int tw = TextLength(rp, (STRPTR)s, strlen(s));

        SetAPen(rp, pens[TEXTPEN]);
        draw_text(rp, l + ((r - l) - tw) / 2, t + ((b - t) - fh) / 2 + base, s);
        return;
    }

    for (i = 0; i < d->n; i++) {
        if (d->pt[i].valid && d->pt[i].value > max) {
            max = d->pt[i].value;
        }
    }
    /* Wie viele Rasterlinien passen? Jede Zahl an der Achse braucht eine
     * Zeile Schrift und ein wenig Luft. Die Hoehe steht erst unten fest,
     * hier reicht die Schaetzung aus der Gesamthoehe. */
    {
        int avail = (b - t) - 2 * fh - 6;
        int fit = avail / (fh + 3);

        step = nice_step(max, fit > 5 ? 5 : fit);
    }
    top   = ((max + step - 1) / step) * step;
    if (top <= 0) {
        top = step;
    }
    lines = (int)(top / step);

    /* Linker Rand: so breit wie die breiteste Achsbeschriftung. */
    for (i = 0; i <= lines; i++) {
        int tw;

        value_label(step * i, step, buf);
        tw = TextLength(rp, (STRPTR)buf, strlen(buf));
        if (tw > lm) {
            lm = tw;
        }
    }

    px0 = l + lm + 4;
    px1 = r;
    py0 = t + fh + 2 + fh / 2;          /* oben Platz fuer die Einheit */
    py1 = b - fh - 3;                   /* unten Platz fuer die Daten */
    if (px1 - px0 < d->n || py1 - py0 < 10) {
        return;                         /* zu klein zum Zeichnen */
    }

    /* Einheit oben links, wie in Home Assistant */
    SetAPen(rp, pens[TEXTPEN]);
    draw_text(rp, px0, t + base, d->unit);

    /* Raster und Achsbeschriftung. Gepunktet, damit die Linien auf dem
     * Grau nicht wie Rahmen aussehen. */
    for (i = 0; i <= lines; i++) {
        int y = py1 - (int)((long)i * (py1 - py0) / lines);
        int tw;

        value_label(step * i, step, buf);
        tw = TextLength(rp, (STRPTR)buf, strlen(buf));
        SetAPen(rp, pens[TEXTPEN]);
        draw_text(rp, px0 - 4 - tw, y - fh / 2 + base, buf);

        SetAPen(rp, pens[SHADOWPEN]);
        SetDrPt(rp, i == 0 ? 0xffff : 0x8888);
        Move(rp, px0, y);
        Draw(rp, px1, y);
    }
    SetDrPt(rp, 0xffff);

    /* Balken. Die Breite rechnet in Tausendsteln Pixel, damit sich 30
     * Balken gleichmaessig auf eine Breite verteilen, die nicht durch 30
     * teilbar ist. */
    {
        long slot = ((long)(px1 - px0 + 1) * 1000L) / d->n;
        int  gap  = slot >= 6000 ? 2 : (slot >= 3000 ? 1 : 0);
        int  every, lw, last_right = -1000;

        /* Beschriftung: nur so oft, wie sie ohne Ueberlappung passt. */
        if (d->period == AH_PERIOD_MONTH) {
            month_name(9, buf, sizeof(buf));
        } else {
            char mon[8];

            month_name(9, mon, sizeof(mon));
            sprintf(buf, GetStr(MSG_CHART_DAY), 28L, mon);
        }
        lw = TextLength(rp, (STRPTR)buf, strlen(buf)) + 6;
        every = (int)(((long)lw * 1000L + slot - 1) / slot);
        if (every < 1) {
            every = 1;
        }

        for (i = 0; i < d->n; i++) {
            int x0 = px0 + (int)(i * slot / 1000) + gap;
            int x1 = px0 + (int)((i + 1) * slot / 1000) - 1 - gap;

            if (x1 < x0) {
                x1 = x0;
            }
            if (d->pt[i].valid && d->pt[i].value > 0) {
                int h = (int)(((long long)d->pt[i].value * (py1 - py0)) / top);

                if (h > 0) {
                    SetAPen(rp, pens[FILLPEN]);
                    RectFill(rp, x0, py1 - h, x1, py1 - 1);
                    if (x1 - x0 >= 3 && h >= 2) {
                        /* Umriss wie bei Home Assistant - die Fuellung
                         * allein verschwimmt bei 8 Farben mit dem Raster */
                        SetAPen(rp, pens[SHINEPEN]);
                        Move(rp, x0, py1 - 1);
                        Draw(rp, x0, py1 - h);
                        Draw(rp, x1, py1 - h);
                        Draw(rp, x1, py1 - 1);
                    }
                }
            }

            /* Datum unter jedem 'every'-ten Balken, gezaehlt vom letzten
             * aus - der juengste Zeitraum ist der interessanteste. */
            if ((d->n - 1 - i) % every == 0) {
                int y, m, dd, tw, cx;
                char mon[8];

                stat_date(d->pt[i].start, &y, &m, &dd);
                month_name(m, mon, sizeof(mon));
                if (d->period == AH_PERIOD_MONTH) {
                    strcpy(buf, mon);
                } else {
                    sprintf(buf, GetStr(MSG_CHART_DAY), (long)dd, mon);
                }
                tw = TextLength(rp, (STRPTR)buf, strlen(buf));
                cx = (x0 + x1) / 2 - tw / 2;
                if (cx < px0) {
                    cx = px0;
                }
                if (cx + tw > px1) {
                    cx = px1 - tw;
                }
                if (cx > last_right + 3) {
                    SetAPen(rp, pens[TEXTPEN]);
                    draw_text(rp, cx, py1 + 2 + base, buf);
                    last_right = cx + tw;
                }
            }
        }
    }
}

/* Register fuer den Dispatcher: MUI ruft ihn mit a0 = Klasse, a2 = Objekt,
 * a1 = Nachricht - wie jeden BOOPSI-Dispatcher. */
static ULONG chart_dispatch(struct IClass *cl __asm("a0"),
                            Object *obj __asm("a2"),
                            Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
        case OM_NEW: {
            Object *o = (Object *)DoSuperMethodA(cl, obj, msg);

            if (o) {
                struct ChartData *d = INST_DATA(cl, o);

                memset(d, 0, sizeof(*d));
            }
            return (ULONG)o;
        }

        case MUIM_AskMinMax: {
            struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;

            DoSuperMethodA(cl, obj, msg);
            m->MinMaxInfo->MinWidth  += 120;
            m->MinMaxInfo->DefWidth  += 300;
            m->MinMaxInfo->MaxWidth  += MUI_MAXMAX;
            m->MinMaxInfo->MinHeight += 90;
            m->MinMaxInfo->DefHeight += 140;
            m->MinMaxInfo->MaxHeight += 220;
            return 0;
        }

        /* MUI_Redraw ist nur erlaubt, solange das Objekt gezeigt wird -
         * Diagramme auf einer anderen Seite bekommen ihre Werte trotzdem,
         * gezeichnet wird dann beim naechsten Zeigen. */
        case MUIM_Show: {
            ULONG rc = DoSuperMethodA(cl, obj, msg);

            ((struct ChartData *)INST_DATA(cl, obj))->shown = TRUE;
            return rc;
        }

        case MUIM_Hide:
            ((struct ChartData *)INST_DATA(cl, obj))->shown = FALSE;
            return DoSuperMethodA(cl, obj, msg);

        case MUIM_Draw: {
            struct MUIP_Draw *m = (struct MUIP_Draw *)msg;

            DoSuperMethodA(cl, obj, msg);
            if (m->flags & MADF_DRAWOBJECT) {
                chart_draw(cl, obj);
            }
            return 0;
        }
    }
    return DoSuperMethodA(cl, obj, msg);
}

BOOL chart_class_open(void)
{
    if (!g_mcc) {
        g_mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
                                      sizeof(struct ChartData),
                                      (APTR)chart_dispatch);
    }
    return (BOOL)(g_mcc != NULL);
}

void chart_class_close(void)
{
    if (g_mcc) {
        MUI_DeleteCustomClass(g_mcc);
        g_mcc = NULL;
    }
}

Object *chart_new(void)
{
    if (!g_mcc) {
        return NULL;
    }
    {
        /* NewObjectA mit fester Liste statt des variadischen NewObject:
         * dieselbe Falle wie bei MUI_NewObject, siehe muistubs.c. */
        static struct TagItem tags[] = {
            { MUIA_Frame,      MUIV_Frame_Text },
            { MUIA_Background, MUII_TextBack },
            { MUIA_FillArea,   TRUE },
            { TAG_DONE,        0 }
        };

        return NewObjectA(g_mcc->mcc_Class, NULL, tags);
    }
}

void chart_set(Object *obj, const struct StatPoint *pts, int n,
               int period, const char *unit)
{
    struct ChartData *d;

    if (!obj || !g_mcc) {
        return;
    }
    d = INST_DATA(g_mcc->mcc_Class, obj);
    if (n > CHART_MAX) {
        pts += n - CHART_MAX;
        n = CHART_MAX;
    }
    memcpy(d->pt, pts, (size_t)n * sizeof(*pts));
    d->n = n;
    d->period = period;
    strncpy(d->unit, unit ? unit : "", sizeof(d->unit) - 1);
    d->unit[sizeof(d->unit) - 1] = '\0';
    if (d->shown) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
}
