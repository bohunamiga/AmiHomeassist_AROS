/* amiloc.c - siehe amiloc.h.
 *
 * Bewusst anspruchslos: ein OpenCatalog beim Start, ein CloseCatalog am
 * Ende, dazwischen nur Zeigerarithmetik. Auf einem 68020 darf eine
 * Beschriftung nichts kosten.
 */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include "amiloc.h"

/* proto/locale.h erwartet genau diesen Typ - nicht struct Library. */
#ifndef __AROS__
struct LocaleBase   *LocaleBase = NULL;
#endif
static struct Catalog *g_catalog = NULL;

void locale_open(void)
{
    char forced[32];

    /* Version 38 = OS 2.1, die erste mit locale.library. Aeltere Systeme
     * bekommen die eingebauten englischen Strings. Auf AROS ist die
     * Bibliothek durch den Auto-Open des Compilers schon offen. */
#ifndef __AROS__
    LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 38);
#endif
    if (!LocaleBase) {
        return;
    }

    /* Eine gesetzte Umgebungsvariable schlaegt die Systemsprache:
     *
     *     SetEnv AmiHomeassistLanguage deutsch
     *
     * Gedacht fuer alle, die ein englisches Workbench fahren, dieses
     * Programm aber in ihrer Sprache wollen - und zum Pruefen der
     * Kataloge, ohne die Locale-Voreinstellungen anzufassen. */
    if (GetVar((STRPTR)"AmiHomeassistLanguage", (STRPTR)forced,
               sizeof(forced) - 1, GVF_GLOBAL_ONLY) > 0) {
        g_catalog = OpenCatalog(NULL, (STRPTR)"AmiHomeassist.catalog",
                                OC_BuiltInLanguage, (IPTR)"english",
                                OC_Language,        (IPTR)forced,
                                TAG_DONE);
    }
    if (!g_catalog) {
        g_catalog = OpenCatalog(NULL, (STRPTR)"AmiHomeassist.catalog",
                                OC_BuiltInLanguage, (IPTR)"english",
                                TAG_DONE);
    }
}

void locale_close(void)
{
    if (LocaleBase) {
        if (g_catalog) {
            CloseCatalog(g_catalog);
            g_catalog = NULL;
        }
#ifndef __AROS__
        CloseLibrary((struct Library *)LocaleBase);
        LocaleBase = NULL;
#endif
    }
}

const char *GetStr(long id)
{
    const char *builtin = AMILOC_BUILTIN[id];

    if (g_catalog) {
        return (const char *)GetCatalogStr(g_catalog, id, (STRPTR)builtin);
    }
    return builtin;
}
