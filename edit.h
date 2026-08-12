/* AmiHomeassist - Dashboard-Editor.
 *
 * Eigene Uebersetzungseinheit, weil gui.c sonst unuebersichtlich wird. Der
 * Editor arbeitet direkt auf dem Modell aus dash.c; die Hauptoberflaeche
 * erfaehrt ueber den Rueckgabewert von editor_handle(), ob sie ihre Seiten
 * neu bauen muss.
 */

#ifndef EDIT_H
#define EDIT_H

#include <exec/types.h>
#include <libraries/mui.h>
#include "amiha.h"
#include "dash.h"

/* Rueckmeldenummern des Editors. gui.c reicht alles in diesem Bereich
 * unbesehen an editor_handle() weiter. */
#define EDIT_ID_FIRST  2000
#define EDIT_ID_LAST   2099

/* Baut die beiden Fenster und haengt sie an die Anwendung. Muss vor dem
 * Oeffnen der Anwendung aufgerufen werden, weil MUI Fenster zur Bauzeit
 * erwartet. */
Object *editor_build(Object *app, struct Dash *d, struct Catalog *c);

void editor_open(void);

/* TRUE, wenn die Nummer zum Editor gehoerte. *changed wird gesetzt, wenn
 * sich am Modell etwas geaendert hat. */
BOOL editor_handle(ULONG id, BOOL *changed);

#endif
