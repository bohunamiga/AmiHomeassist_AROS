/* AmiHomeassist - Kern: Einstellungen, HTTP, Home-Assistant-Zugriff. */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "amiha.h"

struct Library *SocketBase = NULL;

/* IDIR=SASC:netinclude/ steht im Suchpfad vor SAS/Cs eigenem include:, und das
 * AmiTCP-SDK bringt ein eigenes proto/dos.h mit, das DOSBase nicht deklariert.
 * Die Basis selbst legt die SAS/C-Startup an - hier fehlt nur die
 * Bekanntmachung. */
extern struct DosLibrary *DOSBase;

static char g_error[256] = "";

/* Ablaufverfolgung zum Eingrenzen von Haengern in Programmen ohne Konsole.
 * Auf 0 setzen, wenn nicht gebraucht. */
#define AH_TRACE 0

void ah_trace(const char *what, long value)
{
#if AH_TRACE
    BPTR fh = Open((STRPTR)"RAM:amiha.log", MODE_READWRITE);
    if (fh) {
        Seek(fh, 0, OFFSET_END);
        FPrintf(fh, "%s %ld\n", (LONG)(ULONG)what, value);
        Close(fh);
    }
#endif
}

const char *ha_last_error(void)
{
    return g_error[0] ? g_error : "kein Fehler";
}

static int fail(int code, const char *msg)
{
    strncpy(g_error, msg, sizeof(g_error) - 1);
    g_error[sizeof(g_error) - 1] = '\0';
    return code;
}

/* ------------------------------------------------------------------ */
/* Zeichensatz                                                         */
/* ------------------------------------------------------------------ */

void utf8_to_latin1(char *s)
{
    unsigned char *r = (unsigned char *)s;
    unsigned char *w = (unsigned char *)s;

    while (*r) {
        if (*r < 0x80) {
            *w++ = *r++;
        } else if ((*r & 0xE0) == 0xC0 && (r[1] & 0xC0) == 0x80) {
            unsigned int c = ((unsigned int)(*r & 0x1F) << 6) | (r[1] & 0x3F);
            *w++ = (c < 256) ? (unsigned char)c : '?';
            r += 2;
        } else if ((*r & 0xF0) == 0xE0 && (r[1] & 0xC0) == 0x80 &&
                   (r[2] & 0xC0) == 0x80) {
            /* Drei Byte lange Zeichen haben in Latin-1 keine Entsprechung. */
            *w++ = '?';
            r += 3;
        } else {
            *w++ = '?';
            r++;
        }
    }
    *w = '\0';
}

static void trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t') {
        memmove(s, s + 1, strlen(s));
    }
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                     e[-1] == '\r' || e[-1] == '\n')) {
        *--e = '\0';
    }
}

void entity_domain(const char *id, char *out, int outsize)
{
    const char *dot = strchr(id, '.');
    int n = dot ? (int)(dot - id) : (int)strlen(id);

    if (n >= outsize) {
        n = outsize - 1;
    }
    memcpy(out, id, n);
    out[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Einstellungen                                                       */
/* ------------------------------------------------------------------ */

#define PREFS_DIR_ENV    "ENV:AmiHomeassist"
#define PREFS_DIR_ARC    "ENVARC:AmiHomeassist"
#define PREFS_FILE       "AmiHomeassist.prefs"
#define IMPORT_FILE      "Import.prefs"

/* "http://homeassistant:8123" wird zu host="homeassistant", port=8123. */
static int parse_host(struct Prefs *p, const char *value)
{
    const char *s = value;
    char *sep;

    if (strnicmp(s, "https://", 8) == 0) {
        return fail(AH_ENOPREFS,
                    "https wird nicht unterstuetzt - bitte http:// verwenden");
    }
    if (strnicmp(s, "http://", 7) == 0) {
        s += 7;
    }

    strncpy(p->host, s, sizeof(p->host) - 1);
    p->host[sizeof(p->host) - 1] = '\0';

    sep = strchr(p->host, '/');
    if (sep) {
        *sep = '\0';
    }
    sep = strchr(p->host, ':');
    if (sep) {
        *sep = '\0';
        p->port = atoi(sep + 1);
    }
    if (p->port <= 0) {
        p->port = 8123;
    }
    return AH_OK;
}

static int prefs_read_file(struct Prefs *p, const char *path)
{
    BPTR fh;
    char line[700];

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        return 0;
    }

    while (FGets(fh, (STRPTR)line, sizeof(line) - 1)) {
        char *eq;

        trim(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        trim(line);
        trim(eq + 1);

        if (stricmp(line, "host") == 0) {
            parse_host(p, eq + 1);
        } else if (stricmp(line, "token") == 0) {
            strncpy(p->token, eq + 1, sizeof(p->token) - 1);
            p->token[sizeof(p->token) - 1] = '\0';
        } else if (stricmp(line, "poll") == 0) {
            p->poll = atoi(eq + 1);
        }
    }
    Close(fh);
    return 1;
}

int prefs_load(struct Prefs *p)
{
    char path[256];

    memset(p, 0, sizeof(*p));
    p->port = 8123;
    p->poll = 5;

    sprintf(path, "%s/%s", PREFS_DIR_ENV, PREFS_FILE);
    if (!prefs_read_file(p, path)) {
        sprintf(path, "%s/%s", PREFS_DIR_ARC, PREFS_FILE);
        if (!prefs_read_file(p, path)) {
            return fail(AH_ENOPREFS,
                        "keine Einstellungen gefunden "
                        "(ENVARC:AmiHomeassist/AmiHomeassist.prefs)");
        }
    }

    if (p->host[0] == '\0') {
        return fail(AH_ENOPREFS, "in den Einstellungen fehlt host=");
    }
    if (p->token[0] == '\0') {
        return fail(AH_ENOPREFS, "in den Einstellungen fehlt token=");
    }
    if (p->poll <= 0) {
        p->poll = 5;
    }
    return AH_OK;
}

static int prefs_write_one(struct Prefs *p, const char *dir)
{
    BPTR fh, lock;
    char path[256];

    lock = CreateDir((STRPTR)dir);
    if (lock) {
        UnLock(lock);
    }

    sprintf(path, "%s/%s", dir, PREFS_FILE);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        return 0;
    }

    FPrintf(fh, "; AmiHomeassist - Einstellungen\n");
    FPrintf(fh, "; Zeilen mit ; sind Kommentare. Format: schluessel=wert\n\n");
    FPrintf(fh, "host=http://%s:%ld\n", (LONG)(ULONG)p->host, (LONG)p->port);
    FPrintf(fh, "token=%s\n", (LONG)(ULONG)p->token);
    FPrintf(fh, "\n; Abstand der Zustandsabfrage in Sekunden\n");
    FPrintf(fh, "poll=%ld\n", (LONG)p->poll);

    Close(fh);
    return 1;
}

int prefs_save(struct Prefs *p)
{
    int arc = prefs_write_one(p, PREFS_DIR_ARC);

    /* ENV: ist auf manchen Systemen nur eine Verknuepfung auf ENVARC:. Dann
     * schreibt der zweite Aufruf dieselbe Datei noch einmal - nicht schoen,
     * aber harmlos. */
    prefs_write_one(p, PREFS_DIR_ENV);

    if (!arc) {
        return fail(AH_ENOPREFS, "Einstellungen lassen sich nicht schreiben");
    }
    return AH_OK;
}

/* ------------------------------------------------------------------ */
/* Katalog                                                             */
/* ------------------------------------------------------------------ */

void catalog_init(struct Catalog *c)
{
    c->list = NULL;
    c->count = 0;
    c->capacity = 0;
}

void catalog_free(struct Catalog *c)
{
    if (c->list) {
        free(c->list);
    }
    catalog_init(c);
}

static int catalog_room_for_one(struct Catalog *c)
{
    if (c->count < c->capacity) {
        return 1;
    }
    {
        int newcap = c->capacity ? c->capacity * 2 : 128;
        struct Entity *nl = (struct Entity *)
            realloc(c->list, (size_t)newcap * sizeof(struct Entity));

        if (!nl) {
            return 0;
        }
        c->list = nl;
        c->capacity = newcap;
    }
    return 1;
}

struct Entity *catalog_find(struct Catalog *c, const char *id)
{
    int i;

    for (i = 0; i < c->count; i++) {
        if (stricmp(c->list[i].id, id) == 0) {
            return &c->list[i];
        }
    }
    return NULL;
}

int catalog_selected_count(struct Catalog *c)
{
    int i, n = 0;

    for (i = 0; i < c->count; i++) {
        if (c->list[i].selected) {
            n++;
        }
    }
    return n;
}

int import_load(struct Catalog *c, int *found)
{
    BPTR fh;
    char line[ID_LEN + 4];
    char path[256];
    int n = 0;
    int i;

    *found = 0;
    for (i = 0; i < c->count; i++) {
        c->list[i].selected = FALSE;
    }

    sprintf(path, "%s/%s", PREFS_DIR_ARC, IMPORT_FILE);
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        return AH_OK;              /* kein Fehler - nur noch nichts gewaehlt */
    }

    while (FGets(fh, (STRPTR)line, sizeof(line) - 1)) {
        struct Entity *e;

        trim(line);
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }
        n++;
        e = catalog_find(c, line);
        if (e) {
            e->selected = TRUE;
        }
    }
    Close(fh);

    *found = n;
    return AH_OK;
}

int import_save(struct Catalog *c)
{
    BPTR fh, lock;
    char path[256];
    int i;

    lock = CreateDir((STRPTR)PREFS_DIR_ARC);
    if (lock) {
        UnLock(lock);
    }

    sprintf(path, "%s/%s", PREFS_DIR_ARC, IMPORT_FILE);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        return fail(AH_ENOPREFS, "Importliste laesst sich nicht schreiben");
    }

    FPrintf(fh, "; AmiHomeassist - uebernommene Geraete, eine ID je Zeile\n");
    for (i = 0; i < c->count; i++) {
        if (c->list[i].selected) {
            FPrintf(fh, "%s\n", (LONG)(ULONG)c->list[i].id);
        }
    }
    Close(fh);
    return AH_OK;
}

/* ------------------------------------------------------------------ */
/* HTTP                                                                */
/* ------------------------------------------------------------------ */

/* Bewusst HTTP/1.0: dann antwortet der Server nie mit chunked transfer
 * encoding, und der Rumpf ist schlicht alles bis zum Verbindungsende. Das
 * spart einen Parser, den man sonst nur fuer Sonderfaelle braeuchte. */

static int recv_all(int sock, char **out, long *outlen)
{
    long cap = 32768;
    long len = 0;
    char *buf = malloc(cap);
    long n;

    if (!buf) {
        return fail(AH_EMEM, "zu wenig Speicher");
    }

    for (;;) {
        if (len + 4096 >= cap) {
            char *nb = realloc(buf, cap * 2);
            if (!nb) {
                free(buf);
                return fail(AH_EMEM, "zu wenig Speicher");
            }
            buf = nb;
            cap *= 2;
        }
        n = recv(sock, buf + len, cap - len - 1, 0);
        if (n <= 0) {
            break;
        }
        len += n;
    }

    buf[len] = '\0';
    *out = buf;
    *outlen = len;
    return AH_OK;
}

static int http_request(struct Prefs *p, const char *method, const char *path,
                        const char *body, char **resp_body, long *resp_len)
{
    struct hostent *he;
    struct sockaddr_in sa;
    int sock = -1;
    int rc;
    long sent, total, n;
    char *req = NULL;
    char *raw = NULL;
    long rawlen = 0;
    char *hdrend;
    int status = 0;
    long bodylen = body ? (long)strlen(body) : 0;
    long reqcap;

    *resp_body = NULL;
    *resp_len = 0;

    /* net.lib wird bewusst nicht gelinkt - die Basis oeffnen wir selbst.
     * Beides zusammen stuerzt beim Start ab. */
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) {
        return fail(AH_ENET, "bsdsocket.library nicht verfuegbar - "
                             "laeuft der TCP-Stack?");
    }

    he = gethostbyname((UBYTE *)p->host);
    if (!he) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_ENET, "Rechnername laesst sich nicht aufloesen");
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)p->port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_ENET, "socket() fehlgeschlagen");
    }

    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_ENET, "Verbindung abgelehnt - laeuft Home Assistant?");
    }

    reqcap = 1024 + strlen(p->token) + bodylen;
    req = malloc(reqcap);
    if (!req) {
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_EMEM, "zu wenig Speicher");
    }

    sprintf(req,
            "%s %s HTTP/1.0\r\n"
            "Host: %s:%d\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, p->host, p->port, p->token, bodylen);
    if (body) {
        strcat(req, body);
    }

    total = (long)strlen(req);
    sent = 0;
    while (sent < total) {
        n = send(sock, req + sent, total - sent, 0);
        if (n <= 0) {
            free(req);
            CloseSocket(sock);
            CloseLibrary(SocketBase);
            SocketBase = NULL;
            return fail(AH_ENET, "Senden fehlgeschlagen");
        }
        sent += n;
    }
    free(req);

    rc = recv_all(sock, &raw, &rawlen);
    CloseSocket(sock);
    CloseLibrary(SocketBase);
    SocketBase = NULL;

    if (rc != AH_OK) {
        return rc;
    }

    if (sscanf(raw, "HTTP/%*d.%*d %d", &status) != 1) {
        free(raw);
        return fail(AH_EHTTP, "unverstaendliche Antwort vom Server");
    }

    hdrend = strstr(raw, "\r\n\r\n");
    if (!hdrend) {
        free(raw);
        return fail(AH_EHTTP, "Antwort ohne Rumpf");
    }
    hdrend += 4;

    if (status < 200 || status > 299) {
        char msg[128];
        if (status == 401) {
            sprintf(msg, "Home Assistant weist den Token zurueck (401)");
        } else {
            sprintf(msg, "Home Assistant antwortet mit HTTP %d", status);
        }
        free(raw);
        return fail(AH_EHTTP, msg);
    }

    memmove(raw, hdrend, strlen(hdrend) + 1);
    *resp_body = raw;
    *resp_len = (long)strlen(raw);
    return AH_OK;
}

/* ------------------------------------------------------------------ */
/* Abfragen                                                            */
/* ------------------------------------------------------------------ */

/* Die REST-Schnittstelle kennt keine Raeume - die stehen nur in der Registry
 * hinter der WebSocket-Schnittstelle. Statt die nachzubauen, laesst dieses
 * Template Home Assistant die Zuordnung selbst aufloesen und fertige Zeilen
 * liefern. Damit braucht der Amiga weder WebSocket noch JSON-Parser. */
static const char *CATALOG_BODY =
    "{\"template\": \""
    "{%- for s in states if s.domain in [" HA_DOMAINS "] -%}"
    "\\n{{ s.entity_id }}|{{ s.name }}|{{ area_name(s.entity_id) or '-' }}"
    "|{{ s.state }}|{{ s.attributes.unit_of_measurement or '' }}"
    "|{{ s.attributes.device_class or '' }}"
    "|{{ s.attributes.current_position if s.attributes.current_position "
    "is defined else -1 }}"
    "\\n{% endfor -%}"
    "\"}";

static void copy_field(char *dst, int dstsize, const char *src, int len)
{
    if (len >= dstsize) {
        len = dstsize - 1;
    }
    if (len < 0) {
        len = 0;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    trim(dst);
}

/* Zerlegt eine Zeile an '|' und liefert die Anzahl gefundener Felder. */
static int split_fields(char *line, char *field[], int maxfields)
{
    int n = 0;
    char *p = line;

    field[n++] = p;
    while (n < maxfields && (p = strchr(p, '|')) != NULL) {
        *p++ = '\0';
        field[n++] = p;
    }
    return n;
}

int catalog_fetch(struct Prefs *p, struct Catalog *c)
{
    char *body = NULL;
    long len = 0;
    int rc;
    char *line, *next;

    rc = http_request(p, "POST", "/api/template", CATALOG_BODY, &body, &len);
    if (rc != AH_OK) {
        return rc;
    }

    c->count = 0;

    line = body;
    while (line && *line) {
        char *f[7];
        int nf;

        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
        }

        nf = split_fields(line, f, 7);
        if (nf >= 4 && f[0][0]) {
            struct Entity *e;

            if (!catalog_room_for_one(c)) {
                free(body);
                return fail(AH_EMEM, "zu wenig Speicher fuer die Geraeteliste");
            }
            e = &c->list[c->count];
            memset(e, 0, sizeof(*e));

            copy_field(e->id,    ID_LEN,    f[0], (int)strlen(f[0]));
            copy_field(e->name,  NAME_LEN,  f[1], (int)strlen(f[1]));
            copy_field(e->area,  AREA_LEN,  f[2], (int)strlen(f[2]));
            copy_field(e->state, STATE_LEN, f[3], (int)strlen(f[3]));
            if (nf >= 5) {
                copy_field(e->unit, UNIT_LEN, f[4], (int)strlen(f[4]));
            }
            if (nf >= 6) {
                copy_field(e->dclass, DCLASS_LEN, f[5], (int)strlen(f[5]));
            }
            e->pos = -1;
            if (nf >= 7) {
                char tmp[16];

                copy_field(tmp, sizeof(tmp), f[6], (int)strlen(f[6]));
                if (tmp[0]) {
                    e->pos = atoi(tmp);
                }
            }

            utf8_to_latin1(e->name);
            utf8_to_latin1(e->area);
            utf8_to_latin1(e->unit);

            if (e->area[0] == '\0' ||
                (e->area[0] == '-' && e->area[1] == '\0')) {
                strcpy(e->area, AREA_NONE);
            }
            c->count++;
        }

        line = next ? next + 1 : NULL;
    }

    free(body);

    if (c->count == 0) {
        return fail(AH_EHTTP, "Home Assistant liefert keine Geraete");
    }
    return AH_OK;
}

int states_refresh(struct Prefs *p, struct Catalog *c)
{
    char *body = NULL;
    char *resp = NULL;
    long len = 0;
    long cap;
    int rc, i, n = 0;
    char *w;
    BOOL first = TRUE;

    for (i = 0; i < c->count; i++) {
        if (c->list[i].selected) {
            n++;
        }
    }
    if (n == 0) {
        return AH_OK;              /* nichts anzuzeigen, nichts zu holen */
    }

    /* Der Rumpf traegt die IDs als Jinja-Liste. Entity-IDs bestehen nur aus
     * Kleinbuchstaben, Ziffern, Punkt und Unterstrich - da ist nichts zu
     * maskieren, weder fuer JSON noch fuer Jinja. */
    cap = 160 + (long)n * (ID_LEN + 4);
    body = malloc(cap);
    if (!body) {
        return fail(AH_EMEM, "zu wenig Speicher");
    }

    w = body;
    w += sprintf(w, "{\"template\": \"{%%- for e in [");
    for (i = 0; i < c->count; i++) {
        if (c->list[i].selected) {
            w += sprintf(w, "%s'%s'", first ? "" : ",", c->list[i].id);
            first = FALSE;
        }
    }
    sprintf(w, "] -%%}\\n{{ e }}|{{ states(e) }}|{{ state_attr(e,"
               "'current_position') if state_attr(e, 'current_position') "
               "is not none else -1 }}\\n{%% endfor -%%}\"}");

    rc = http_request(p, "POST", "/api/template", body, &resp, &len);
    free(body);
    if (rc != AH_OK) {
        return rc;
    }

    {
        char *line = resp;
        char *next;

        while (line && *line) {
            next = strchr(line, '\n');
            if (next) {
                *next = '\0';
            }
            {
                char *f3[3];
                int nf = split_fields(line, f3, 3);

                if (nf >= 2 && f3[0][0]) {
                    struct Entity *e;

                    trim(f3[0]);
                    e = catalog_find(c, f3[0]);
                    if (e) {
                        copy_field(e->state, STATE_LEN, f3[1],
                                   (int)strlen(f3[1]));
                        if (nf >= 3) {
                            char tmp[16];

                            copy_field(tmp, sizeof(tmp), f3[2],
                                       (int)strlen(f3[2]));
                            if (tmp[0]) {
                                e->pos = atoi(tmp);
                            }
                        }
                    }
                }
            }
            line = next ? next + 1 : NULL;
        }
    }

    free(resp);
    return AH_OK;
}

int ha_service(struct Prefs *p, const char *entity_id, const char *service)
{
    char path[128];
    char bodybuf[ID_LEN + 32];
    char domain[32];
    char *body = NULL;
    long len = 0;
    int rc;

    entity_domain(entity_id, domain, sizeof(domain));
    if (domain[0] == '\0') {
        return fail(AH_EHTTP, "unbrauchbare Entity-ID");
    }

    sprintf(path, "/api/services/%s/%s", domain, service);
    sprintf(bodybuf, "{\"entity_id\": \"%s\"}", entity_id);

    rc = http_request(p, "POST", path, bodybuf, &body, &len);
    if (body) {
        free(body);
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* Filter und Sortierung                                               */
/* ------------------------------------------------------------------ */

static BOOL ends_with(const char *s, const char *suffix)
{
    int ls = (int)strlen(s);
    int lf = (int)strlen(suffix);

    return (BOOL)(ls >= lf && strcmp(s + ls - lf, suffix) == 0);
}

BOOL entity_is_noise(const struct Entity *e)
{
    /* Die Fritzbox legt pro Netzgeraet einen Schalter an - allein davon gab es
     * in der Testanlage 78 von 141 Eintraegen. */
    if (strstr(e->name, "Internet access")) {
        return TRUE;
    }
    /* Meross-Stecker melden ihr Kontrolllaempchen als eigene Lampe.
     * "light.miner_messen_dnd" ist keine Lampe. */
    if (ends_with(e->name, " Dnd")) {
        return TRUE;
    }
    if (strstr(e->name, "overtemp") || strstr(e->name, "Api Usage")) {
        return TRUE;
    }
    if (stricmp(e->state, "unavailable") == 0) {
        return TRUE;
    }
    return FALSE;
}

static int cmp_entity(const void *a, const void *b)
{
    const struct Entity *ea = (const struct Entity *)a;
    const struct Entity *eb = (const struct Entity *)b;
    int na = (strcmp(ea->area, AREA_NONE) == 0);
    int nb = (strcmp(eb->area, AREA_NONE) == 0);
    int r;

    if (na != nb) {
        return na - nb;            /* "Ohne Raum" ans Ende */
    }
    r = stricmp(ea->area, eb->area);
    if (r) {
        return r;
    }
    return stricmp(ea->name, eb->name);
}

void catalog_sort(struct Catalog *c)
{
    qsort(c->list, c->count, sizeof(struct Entity), cmp_entity);
}
