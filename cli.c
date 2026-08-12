/* AmiHomeassist - Kommandozeilenfassung.
 *
 * Enthaelt bewusst keine Logik ausser Ein- und Ausgabe: alles Inhaltliche
 * steckt in amiha.c, damit die MUI-Oberflaeche denselben Kern benutzt und
 * nichts doppelt gepflegt werden muss.
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>
#include <dos/rdargs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiha.h"
#include "dash.h"

/* siehe amiha.c: netinclude verdeckt SAS/Cs proto/dos.h */
extern struct DosLibrary *DOSBase;

const char *VERSTAG = "$VER: AmiHomeassist 0.5 (11.8.2026)";

#define TEMPLATE "ALL/S,DOMAIN/K,ON/K,OFF/K,TOGGLE/K,STATES/S,DASH/S,HOST/K,TOKEN/K,SAVE/S"

enum {
    ARG_ALL, ARG_DOMAIN, ARG_ON, ARG_OFF, ARG_TOGGLE, ARG_STATES, ARG_DASH,
    ARG_HOST, ARG_TOKEN, ARG_SAVE, ARG_COUNT
};

static struct Catalog g_cat;

static BOOL is_switchable(const struct Entity *e)
{
    char dom[24];

    entity_domain(e->id, dom, sizeof(dom));
    return (BOOL)(strcmp(dom, "light") == 0 || strcmp(dom, "switch") == 0);
}

static void print_list(struct Prefs *p, BOOL show_all, const char *domain)
{
    int rc, i, shown = 0, hidden = 0;
    char current[AREA_LEN];

    rc = catalog_fetch(p, &g_cat);
    if (rc != AH_OK) {
        printf("Fehler: %s\n", ha_last_error());
        return;
    }
    catalog_sort(&g_cat);

    current[0] = '\0';
    for (i = 0; i < g_cat.count; i++) {
        struct Entity *e = &g_cat.list[i];
        char dom[24];
        char value[STATE_LEN + UNIT_LEN + 4];

        entity_domain(e->id, dom, sizeof(dom));
        if (domain && stricmp(dom, domain) != 0) {
            continue;
        }
        if (!show_all && entity_is_noise(e)) {
            hidden++;
            continue;
        }

        if (strcmp(current, e->area) != 0) {
            strcpy(current, e->area);
            printf("\n%s\n", current);
        }

        if (is_switchable(e)) {
            sprintf(value, "%s", (stricmp(e->state, "on") == 0) ? "an" : "aus");
        } else if (e->unit[0]) {
            sprintf(value, "%s %s", e->state, e->unit);
        } else {
            sprintf(value, "%s", e->state);
        }

        printf("  %-30s %14s  %s\n", e->name, value, e->id);
        shown++;
    }

    printf("\n%d von %d Eintraegen", shown, g_cat.count);
    if (hidden) {
        printf(", %d ausgeblendet (mit ALL sichtbar)", hidden);
    }
    printf("\n");
}

/* Zeigt, was der Sparbetrieb bringt: erst der Katalog, dann nur die
 * uebernommenen Geraete. */
static void print_states(struct Prefs *p)
{
    int rc, found, i, n;

    rc = catalog_fetch(p, &g_cat);
    if (rc != AH_OK) {
        printf("Fehler: %s\n", ha_last_error());
        return;
    }
    catalog_sort(&g_cat);
    import_load(&g_cat, &found);
    n = catalog_selected_count(&g_cat);

    printf("Katalog: %d Eintraege, davon %d uebernommen\n\n",
           g_cat.count, n);
    if (n == 0) {
        printf("Nichts uebernommen - erst in der Oberflaeche auswaehlen.\n");
        return;
    }

    rc = states_refresh(p, &g_cat);
    if (rc != AH_OK) {
        printf("Fehler: %s\n", ha_last_error());
        return;
    }

    for (i = 0; i < g_cat.count; i++) {
        struct Entity *e = &g_cat.list[i];

        if (e->selected) {
            printf("  %-30s %-10s %-6s %s\n",
                   e->name, e->state, e->unit, e->id);
        }
    }
}

/* Legt aus der Auswahl Dashboards an, schreibt sie, liest sie zurueck und
 * zeigt das Ergebnis. Damit ist der ganze Weg Modell-Datei-Modell geprueft. */
static void do_dash(struct Prefs *p)
{
    struct Dash d;
    int rc, found, i, j, k;
    static const char *kn[WK_COUNT] = {
        "toggle", "lamp", "value", "gauge", "cover", "text"
    };

    rc = catalog_fetch(p, &g_cat);
    if (rc != AH_OK) {
        printf("Fehler: %s\n", ha_last_error());
        return;
    }
    catalog_sort(&g_cat);
    import_load(&g_cat, &found);

    dash_init(&d);
    dash_generate(&d, &g_cat);
    printf("erzeugt: %d Seiten aus %d uebernommenen Geraeten\n",
           d.count, catalog_selected_count(&g_cat));

    if (dash_save(&d) != AH_OK) {
        printf("Fehler beim Schreiben.\n");
        dash_free(&d);
        return;
    }
    dash_free(&d);

    dash_init(&d);
    if (dash_load(&d) != AH_OK) {
        printf("Fehler beim Lesen.\n");
        return;
    }
    printf("zurueckgelesen: %d Seiten\n\n", d.count);

    for (i = 0; i < d.count; i++) {
        printf("%s  (Symbol %d)\n", d.p[i].title, d.p[i].icon);
        for (j = 0; j < d.p[i].count; j++) {
            printf("  [%s]\n", d.p[i].g[j].title);
            for (k = 0; k < d.p[i].g[j].count; k++) {
                struct Widget *w = &d.p[i].g[j].w[k];

                if (w->kind == WK_GAUGE) {
                    printf("    %-7s %-28s %ld..%ld\n", kn[w->kind],
                           w->label, w->min, w->max);
                } else {
                    printf("    %-7s %-28s %s\n", kn[w->kind],
                           w->label, w->id);
                }
            }
        }
    }

    dash_mark_used(&d, &g_cat);
    printf("\nim Takt abzufragen: %d Geraete\n",
           catalog_selected_count(&g_cat));
    dash_free(&d);
}

static void do_service(struct Prefs *p, const char *entity, const char *service,
                       const char *wort)
{
    if (ha_service(p, entity, service) == AH_OK) {
        printf("%s: %s\n", entity, wort);
    } else {
        printf("Fehler: %s\n", ha_last_error());
    }
}

int main(void)
{
    struct RDArgs *rda;
    LONG args[ARG_COUNT];
    struct Prefs prefs;
    int rc;

    memset(args, 0, sizeof(args));
    catalog_init(&g_cat);

    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"AmiHomeassist");
        return RETURN_FAIL;
    }

    rc = prefs_load(&prefs);

    /* HOST und TOKEN duerfen die Einstellungen ueberschreiben - so laesst sich
     * das Programm auf einer fremden Maschine einrichten, ohne die Datei von
     * Hand anzulegen. */
    if (args[ARG_HOST] || args[ARG_TOKEN]) {
        if (args[ARG_HOST]) {
            char tmp[200];
            char *colon;

            strncpy(tmp, (char *)args[ARG_HOST], sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            if (strnicmp(tmp, "http://", 7) == 0) {
                memmove(tmp, tmp + 7, strlen(tmp + 7) + 1);
            }
            prefs.port = 8123;
            colon = strchr(tmp, ':');
            if (colon) {
                *colon = '\0';
                prefs.port = atoi(colon + 1);
                if (prefs.port <= 0) {
                    prefs.port = 8123;
                }
            }
            strncpy(prefs.host, tmp, sizeof(prefs.host) - 1);
            prefs.host[sizeof(prefs.host) - 1] = '\0';
        }
        if (args[ARG_TOKEN]) {
            strncpy(prefs.token, (char *)args[ARG_TOKEN],
                    sizeof(prefs.token) - 1);
            prefs.token[sizeof(prefs.token) - 1] = '\0';
        }
        if (prefs.poll <= 0) {
            prefs.poll = 5;
        }
        rc = AH_OK;

        if (args[ARG_SAVE]) {
            if (prefs_save(&prefs) == AH_OK) {
                printf("Einstellungen gespeichert.\n");
            } else {
                printf("Fehler: %s\n", ha_last_error());
            }
        }
    }

    if (rc != AH_OK) {
        printf("Fehler: %s\n", ha_last_error());
        printf("\nEinrichten zum Beispiel so:\n");
        printf("  AmiHomeassist HOST=http://homeassistant:8123 "
               "TOKEN=<dein-token> SAVE\n");
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    if (args[ARG_ON]) {
        do_service(&prefs, (char *)args[ARG_ON], "turn_on", "eingeschaltet");
    } else if (args[ARG_OFF]) {
        do_service(&prefs, (char *)args[ARG_OFF], "turn_off", "ausgeschaltet");
    } else if (args[ARG_TOGGLE]) {
        do_service(&prefs, (char *)args[ARG_TOGGLE], "toggle", "umgeschaltet");
    } else if (args[ARG_STATES]) {
        print_states(&prefs);
    } else if (args[ARG_DASH]) {
        do_dash(&prefs);
    } else {
        print_list(&prefs, args[ARG_ALL] ? TRUE : FALSE,
                   (char *)args[ARG_DOMAIN]);
    }

    catalog_free(&g_cat);
    FreeArgs(rda);
    return RETURN_OK;
}
