/* amiloc.h - Sprachumschaltung ueber locale.library.
 *
 * Die eingebauten Strings sind ENGLISCH. Deutsch, Italienisch und Spanisch
 * kommen aus Katalogen unter LOCALE:Catalogs/<sprache>/AmiHomeassist.catalog.
 * Fehlt locale.library (Kickstart unter 2.1) oder passt kein Katalog, laeuft
 * alles auf Englisch weiter - ohne Fehlermeldung und ohne Kosten.
 *
 * Die MSG_-Nummern stehen in locale_strings.h, das locale.py aus strings.cd
 * erzeugt. Nie von Hand aendern.
 */
#ifndef AMILOC_H
#define AMILOC_H

#include "locale_strings.h"

void        locale_open(void);
void        locale_close(void);
const char *GetStr(long id);

#endif
