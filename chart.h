/* AmiHomeassist - Balkendiagramm als eigene MUI-Klasse.
 *
 * MUI 3.8 bringt keine Diagrammklasse mit, und eine Fremdklasse muesste
 * jeder Anwender erst installieren. Diese hier ist eine Unterklasse von
 * Area und zeichnet selbst: Balken mit RectFill, Raster und Achse mit
 * Move/Draw, Beschriftung mit Text - alles ganze Zahlen, keine FPU.
 */

#ifndef CHART_H
#define CHART_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include "amiha.h"

#define CHART_MAX 60            /* mehr Balken passen ohnehin nicht */

/* Einmal vor dem ersten Objekt, einmal nach dem letzten. */
BOOL    chart_class_open(void);
void    chart_class_close(void);

Object *chart_new(void);

/* Neue Werte uebernehmen und neu zeichnen. Die Punkte werden kopiert.
 * n == 0 zeigt "keine Daten". */
void    chart_set(Object *obj, const struct StatPoint *pts, int n,
                  int period, const char *unit);

#endif
