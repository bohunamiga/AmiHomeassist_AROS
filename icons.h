/* Bilddaten fuer die Zeilen der Geraeteliste.
 * Der Inhalt von icons.c wird von icons.py aus ASCII-Zeichnungen erzeugt. */

#ifndef ICONS_H
#define ICONS_H

#include <exec/types.h>

struct IconDef {
    const UBYTE *body;      /* ILBM-BODY, Zeile fuer Zeile, Plane fuer Plane */
    WORD         width;
    WORD         height;
    WORD         depth;
};

extern const ULONG icon_colors[12];

extern const struct IconDef icon_switch_off;
extern const struct IconDef icon_switch_on;
extern const struct IconDef icon_lamp;
extern const struct IconDef icon_plug;
extern const struct IconDef icon_check_off;
extern const struct IconDef icon_check_on;

/* Die Raumsymbole der Seitenleiste. Erzeugt von mdi.py aus den Material
 * Design Icons (Apache 2.0), 24x24 in 16 Farben. Eigene Palette, deshalb
 * mdi_colors statt icon_colors. */
#define MDI_COUNT 36

extern const ULONG mdi_colors[48];

/* Liefert das Symbol zu einer Bildnummer aus dash.c/icon_for_area(). */
const struct IconDef *mdi_icon(int n);

/* Das Home-Assistant-Logo, 48x48 in 16 Farben, aus HA_Logo.webp erzeugt.
 * Eigene Palette, deshalb logo_colors. */
extern const ULONG logo_colors[48];
extern const struct IconDef logo_image;

/* Bildnummern, wie sie im Zeilentext als \33o[<n>] auftauchen. */
#define IMG_SWITCH_OFF  0
#define IMG_SWITCH_ON   1
#define IMG_LAMP        2
#define IMG_PLUG        3
#define IMG_CHECK_OFF   4
#define IMG_CHECK_ON    5
#define IMG_COUNT       6

#endif
