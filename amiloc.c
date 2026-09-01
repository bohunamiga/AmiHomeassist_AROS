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
struct LocaleBase   *LocaleBase = NULL;
static struct Catalog *g_catalog = NULL;

void locale_open(void)
{
    char forced[32];

    /* Version 38 = OS 2.1, die erste mit locale.library. Aeltere Systeme
     * bekommen die eingebauten englischen Strings. */
    LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 38);
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
                                OC_BuiltInLanguage, (Tag)"english",
                                OC_Language,        (Tag)forced,
                                TAG_DONE);
    }
    if (!g_catalog) {
        g_catalog = OpenCatalog(NULL, (STRPTR)"AmiHomeassist.catalog",
                                OC_BuiltInLanguage, (Tag)"english",
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
        CloseLibrary((struct Library *)LocaleBase);
        LocaleBase = NULL;
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
