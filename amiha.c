/* AmiHomeassist - Kern: Einstellungen, HTTP, Home-Assistant-Zugriff. */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "amiha.h"
#include "amiloc.h"

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
    return g_error[0] ? g_error : GetStr(MSG_ERR_NONE);
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
                    GetStr(MSG_ERR_NOHTTPS));
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
        } else if (stricmp(line, "unknown") == 0) {
            p->unknown = atoi(eq + 1);
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
    p->unknown = AH_UNK_DIM;

    sprintf(path, "%s/%s", PREFS_DIR_ENV, PREFS_FILE);
    if (!prefs_read_file(p, path)) {
        sprintf(path, "%s/%s", PREFS_DIR_ARC, PREFS_FILE);
        if (!prefs_read_file(p, path)) {
            return fail(AH_ENOPREFS,
                        GetStr(MSG_ERR_NOPREFS));
        }
    }

    if (p->host[0] == '\0') {
        return fail(AH_ENOPREFS, GetStr(MSG_ERR_NOHOST));
    }
    if (p->token[0] == '\0') {
        return fail(AH_ENOPREFS, GetStr(MSG_ERR_NOTOKEN));
    }
    if (p->poll <= 0) {
        p->poll = 5;
    }
    if (p->unknown < AH_UNK_SHOW || p->unknown > AH_UNK_HIDE) {
        p->unknown = AH_UNK_DIM;
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

    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_PREFS_HEAD));
    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_PREFS_NOTE));
    FPrintf(fh, "host=http://%s:%ld\n", (LONG)(ULONG)p->host, (LONG)p->port);
    FPrintf(fh, "token=%s\n", (LONG)(ULONG)p->token);
    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_PREFS_POLL));
    FPrintf(fh, "poll=%ld\n", (LONG)p->poll);
    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_PREFS_UNKNOWN));
    FPrintf(fh, "unknown=%ld\n", (LONG)p->unknown);

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
        return fail(AH_ENOPREFS, GetStr(MSG_ERR_PREFSWRITE));
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
        return fail(AH_ENOPREFS, GetStr(MSG_ERR_IMPORTWRITE));
    }

    FPrintf(fh, "%s", (LONG)(ULONG)GetStr(MSG_FILE_IMPORT_HEAD));
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

/* Zeitgrenzen in Sekunden. Ohne sie haengt das GANZE Programm am TCP-Stack:
 * der Verbindungsaufbau zu einem Rechner, der nicht antwortet, laeuft bei
 * Roadshow rund 75 Sekunden, und weil die Abfrage im selben Task wie die
 * Oberflaeche laeuft, ist derweil kein Fenster, kein Menue und kein Ctrl-C
 * zu gebrauchen. Home Assistant startet nach jedem Update neu, das WLAN
 * zuckt - der Fall ist Alltag, nicht Ausnahme. */
#define AH_CONNECT_SECS  5
#define AH_IO_SECS      15

/* bsdsocket.library zaehlt Fehler wie BSD und NICHT wie das errno.h der
 * benutzten C-Bibliothek - deshalb stehen die beiden Werte hier selbst,
 * statt sich aus libnix zu bedienen. */
#define AH_EWOULDBLOCK  35
#define AH_EINPROGRESS  36

/* Wartet, bis der Socket lesbar (oder schreibbar) ist.
 * 1 = bereit, 0 = Zeit abgelaufen, -1 = Fehler. */
static int sock_wait(int sock, BOOL forwrite, long secs)
{
    fd_set fds;
    struct timeval tv;
    long n;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec  = secs;
    tv.tv_usec = 0;

    /* Der Zeiger geht als APTR hinein: bsdsocket erwartet 'struct __timeval',
     * eine Typangabe, die kein Header ausfuellt. */
    n = WaitSelect(sock + 1,
                   forwrite ? NULL : (APTR)&fds,
                   forwrite ? (APTR)&fds : NULL,
                   NULL, (APTR)&tv, NULL);
    if (n < 0) {
        return -1;
    }
    return n > 0 ? 1 : 0;
}

/* Verbindet mit Zeitgrenze. Der Socket ist dazu nicht blockierend gestellt
 * und bleibt es auch danach - send() und recv() unten warten selbst. */
static int connect_timeout(int sock, struct sockaddr_in *sa)
{
    LONG nb = 1;
    LONG err = 0;
    socklen_t errlen = sizeof(err);
    int w;

    IoctlSocket(sock, FIONBIO, (APTR)&nb);

    if (connect(sock, (struct sockaddr *)sa, sizeof(*sa)) == 0) {
        return AH_OK;
    }
    if (Errno() != AH_EINPROGRESS && Errno() != AH_EWOULDBLOCK) {
        return fail(AH_ENET, GetStr(MSG_ERR_REFUSED));
    }

    w = sock_wait(sock, TRUE, AH_CONNECT_SECS);
    if (w == 0) {
        return fail(AH_ENET, GetStr(MSG_ERR_TIMEOUT));
    }
    if (w < 0) {
        return fail(AH_ENET, GetStr(MSG_ERR_REFUSED));
    }

    /* Schreibbar heisst nur "fertig", nicht "geglueckt" - abgewiesene
     * Verbindungen melden sich genauso. Der Grund steht in SO_ERROR. */
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (APTR)&err, &errlen) < 0
            || err != 0) {
        return fail(AH_ENET, GetStr(MSG_ERR_REFUSED));
    }
    return AH_OK;
}

static int recv_all(int sock, char **out, long *outlen)
{
    long cap = 32768;
    long len = 0;
    char *buf = malloc(cap);
    long n;

    if (!buf) {
        return fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
    }

    for (;;) {
        int w;

        if (len + 4096 >= cap) {
            char *nb = realloc(buf, cap * 2);
            if (!nb) {
                free(buf);
                return fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
            }
            buf = nb;
            cap *= 2;
        }

        /* Der Socket ist nicht blockierend, also erst warten, bis wirklich
         * etwas da ist. Sonst kaeme recv() sofort mit EWOULDBLOCK zurueck
         * und die Schleife liefe heiss. */
        w = sock_wait(sock, FALSE, AH_IO_SECS);
        if (w == 0) {
            free(buf);
            return fail(AH_ENET, GetStr(MSG_ERR_TIMEOUT));
        }
        if (w < 0) {
            free(buf);
            return fail(AH_ENET, GetStr(MSG_ERR_RECV));
        }

        n = recv(sock, buf + len, cap - len - 1, 0);
        if (n < 0 && Errno() == AH_EWOULDBLOCK) {
            continue;               /* Fehlalarm - weiter warten */
        }
        if (n <= 0) {
            break;                  /* 0 = Gegenstelle hat zugemacht */
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
    in_addr_t addr;
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
        return fail(AH_ENET, GetStr(MSG_ERR_NOSOCKET));
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)p->port);

    /* Steht in den Einstellungen eine Zahlenadresse, ist der Namensdienst
     * ueberfluessig. Das spart nicht nur eine Anfrage je Abfrage, es nimmt
     * auch die einzige Wartestelle heraus, die sich nicht begrenzen laesst:
     * gethostbyname() blockiert, so lange der Resolver will. */
    addr = inet_addr((STRPTR)p->host);
    if (addr != INADDR_NONE) {
        sa.sin_addr.s_addr = addr;
    } else {
        he = gethostbyname((UBYTE *)p->host);
        if (!he) {
            CloseLibrary(SocketBase);
            SocketBase = NULL;
            return fail(AH_ENET, GetStr(MSG_ERR_NORESOLVE));
        }
        memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_ENET, GetStr(MSG_ERR_SOCKET));
    }

    rc = connect_timeout(sock, &sa);
    if (rc != AH_OK) {
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return rc;              /* Meldung steht schon */
    }

    reqcap = 1024 + strlen(p->token) + bodylen;
    req = malloc(reqcap);
    if (!req) {
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
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
        int w = sock_wait(sock, TRUE, AH_IO_SECS);

        if (w > 0) {
            n = send(sock, req + sent, total - sent, 0);
            if (n < 0 && Errno() == AH_EWOULDBLOCK) {
                continue;
            }
        } else {
            n = -1;
        }
        if (n <= 0) {
            const char *msg = (w == 0) ? GetStr(MSG_ERR_TIMEOUT)
                                       : GetStr(MSG_ERR_SEND);
            free(req);
            CloseSocket(sock);
            CloseLibrary(SocketBase);
            SocketBase = NULL;
            return fail(AH_ENET, msg);
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
        return fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
    }

    hdrend = strstr(raw, "\r\n\r\n");
    if (!hdrend) {
        free(raw);
        return fail(AH_EHTTP, GetStr(MSG_ERR_NOBODY));
    }
    hdrend += 4;

    if (status < 200 || status > 299) {
        char msg[128];
        if (status == 401) {
            strcpy(msg, GetStr(MSG_ERR_TOKEN401));
        } else {
            sprintf(msg, GetStr(MSG_ERR_HTTP), status);
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
/* Ein Jinja-Ausdruck, der ein Attribut in Zehntelgrad ausgibt - oder den
 * Platzhalter, wenn es das Attribut nicht gibt. Als Makro, weil er siebenmal
 * gebraucht wird und ausgeschrieben nicht mehr zu lesen waere.
 *
 * Bewusst state_attr() statt s.attributes.x: fehlt das Attribut, liefert
 * state_attr None, waehrend s.attributes.x ein Undefined liefert - und
 * Undefined mal zehn ist ein Fehler, der die ganze Antwort verdirbt. */
#define AH_T10(attr) \
    "{{ ((" attr " * 10) | round | int) if " attr " is not none else -32768 }}"

static const char *CATALOG_BODY =
    "{\"template\": \""
    "{%- for s in states if s.domain in [" HA_DOMAINS "] -%}"
    "\\n{{ s.entity_id }}|{{ s.name }}|{{ area_name(s.entity_id) or '-' }}"
    /* Die beiden Attribute ueber state_attr, nicht ueber s.attributes.x:
     * fehlt das Attribut, schreibt Home Assistant sonst je Entitaet und
     * Abfrage eine Warnung ins Protokoll. Bei 700 Entitaeten im Sekundentakt
     * sind das Tausende - am 01.09.2026 standen 9600 Stueck darin. */
    "|{{ s.state }}|{{ state_attr(s.entity_id,'unit_of_measurement') or '' }}"
    "|{{ state_attr(s.entity_id,'device_class') or '' }}"
    "|{{ state_attr(s.entity_id,'current_position') "
    "if state_attr(s.entity_id,'current_position') is not none else -1 }}"
    /* Nur Heizungen haengen fuenf weitere Felder an: Ist, Soll, Grenzen und
     * Schrittweite in Zehntelgrad, dazu die Betriebsarten. Die 700 anderen
     * Zeilen bleiben dadurch so kurz wie bisher. */
    "{%- if s.domain == 'climate' -%}"
    "|" AH_T10("state_attr(s.entity_id,'current_temperature')")
    "|" AH_T10("state_attr(s.entity_id,'temperature')")
    "|" AH_T10("state_attr(s.entity_id,'min_temp')")
    "|" AH_T10("state_attr(s.entity_id,'max_temp')")
    "|" AH_T10("state_attr(s.entity_id,'target_temp_step')")
    "|{{ state_attr(s.entity_id,'hvac_modes') | join(',') "
    "if state_attr(s.entity_id,'hvac_modes') is not none else '' }}"
    /* Das schliessende endif OHNE Strich hinten: mit "-%}" frisst Jinja die
     * Zeilenschaltung dahinter, und die ist der Trenner zwischen zwei
     * Geraeten. Dann kommt die ganze Anlage als eine einzige Zeile an und
     * genau ein Geraet ueberlebt das Einlesen. */
    "{%- endif %}"
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

/* Ein Zahlenfeld aus der Antwort, in Zehntelgrad. Leer oder Platzhalter
 * heisst: gibt es nicht. */
static int field_tenths(const char *src)
{
    char tmp[16];

    copy_field(tmp, sizeof(tmp), src, (int)strlen(src));
    if (tmp[0] == '\0') {
        return TEMP_NONE;
    }
    return atoi(tmp);
}

/* Die Liste der Betriebsarten kann laenger sein als das Feld. Abgeschnitten
 * bliebe sonst ein halbes Wort stehen ("off,heat,coo"), und das waere eine
 * Betriebsart, die Home Assistant nicht kennt - also das angebrochene letzte
 * Stueck wegwerfen. */
static void trim_partial_mode(char *modes)
{
    int n = (int)strlen(modes);

    if (n < MODES_LEN - 1) {
        return;                    /* nichts abgeschnitten */
    }
    while (n > 0 && modes[n - 1] != ',') {
        n--;
    }
    if (n > 0) {
        modes[n - 1] = '\0';      /* das Komma mit weg */
    }
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
        char *f[13];
        int nf;

        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
        }

        nf = split_fields(line, f, 13);
        if (nf >= 4 && f[0][0]) {
            struct Entity *e;

            if (!catalog_room_for_one(c)) {
                free(body);
                return fail(AH_EMEM, GetStr(MSG_ERR_NOMEMLIST));
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

            /* Heizungen haengen fuenf Zahlen und die Betriebsarten an.
             * Alle anderen Zeilen hoeren nach Feld 7 auf. */
            e->cur = e->tgt = TEMP_NONE;
            e->tmin = e->tmax = e->tstep = TEMP_NONE;
            if (nf >= 12) {
                e->cur   = (short)field_tenths(f[7]);
                e->tgt   = (short)field_tenths(f[8]);
                e->tmin  = (short)field_tenths(f[9]);
                e->tmax  = (short)field_tenths(f[10]);
                e->tstep = (short)field_tenths(f[11]);
                if (nf >= 13) {
                    copy_field(e->modes, MODES_LEN, f[12],
                               (int)strlen(f[12]));
                    trim_partial_mode(e->modes);
                }
                /* Ohne brauchbare Grenzen waere das Bedienelement gefaehrlich:
                 * lieber die HA-Voreinstellungen als gar nichts. */
                if (e->tmin == TEMP_NONE || e->tmax == TEMP_NONE ||
                        e->tmin >= e->tmax) {
                    e->tmin = 70;
                    e->tmax = 300;
                }
                if (e->tstep == TEMP_NONE || e->tstep <= 0) {
                    e->tstep = 5;
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
        return fail(AH_EHTTP, GetStr(MSG_ERR_NODEVICES));
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
     * maskieren, weder fuer JSON noch fuer Jinja.
     *
     * Der feste Teil (Kopf plus Jinja-Schwanz) ist rund 460 Zeichen lang.
     * Frueher standen hier nur 160: das ging nur gut, weil kurze IDs in
     * ihren ID_LEN + 4 Platz uebrig liessen. Bei einem bis vier Geraeten
     * schrieb sprintf hinter den Puffer. */
    cap = 1024 + (long)n * (ID_LEN + 4);
    body = malloc(cap);
    if (!body) {
        return fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
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
               "is not none else -1 }}"
               /* Nur Heizungen: Ist und Soll. Die Grenzen stehen schon im
                * Katalog und aendern sich nicht. */
               "{%%- if e.startswith('climate.') -%%}"
               "|" AH_T10("state_attr(e,'current_temperature')")
               "|" AH_T10("state_attr(e,'temperature')")
               "{%%- endif %%}"     /* kein Strich - siehe CATALOG_BODY */
               "\\n{%% endfor -%%}\"}");

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
                char *f3[5];
                int nf = split_fields(line, f3, 5);

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
                        if (nf >= 5) {
                            e->cur = (short)field_tenths(f3[3]);
                            e->tgt = (short)field_tenths(f3[4]);
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
        return fail(AH_EHTTP, GetStr(MSG_ERR_BADENTITY));
    }

    sprintf(path, "/api/services/%s/%s", domain, service);
    sprintf(bodybuf, "{\"entity_id\": \"%s\"}", entity_id);

    rc = http_request(p, "POST", path, bodybuf, &body, &len);
    if (body) {
        free(body);
    }
    return rc;
}

void temp_text(int tenths, char *out, int outsize)
{
    int whole, frac;

    if (tenths == TEMP_NONE || outsize < 8) {
        if (outsize > 0) {
            out[0] = '\0';
        }
        return;
    }
    whole = tenths / 10;
    frac  = tenths % 10;
    if (frac < 0) {
        frac = -frac;
    }
    /* -0,5 Grad ist whole == 0 und trotzdem negativ - das Minus muss von
     * Hand davor, sonst stuende da 0.5. */
    if (tenths < 0 && whole == 0) {
        sprintf(out, "-0.%d", frac);
    } else {
        sprintf(out, "%d.%d", whole, frac);
    }
}

BOOL hvac_next_mode(const struct Entity *e, char *out, int outsize)
{
    const char *p = e->modes;
    const char *first = NULL;
    int firstlen = 0;
    BOOL take_next = FALSE;

    if (!p || !*p) {
        return FALSE;
    }

    /* Einmal durch die Liste: das Stueck hinter dem jetzigen Zustand ist das
     * gesuchte, und ist der jetzige das letzte, faengt es wieder vorn an. */
    while (*p) {
        const char *comma = strchr(p, ',');
        int len = comma ? (int)(comma - p) : (int)strlen(p);

        if (!first) {
            first = p;
            firstlen = len;
        }
        if (take_next) {
            if (len >= outsize) {
                len = outsize - 1;
            }
            memcpy(out, p, len);
            out[len] = '\0';
            return TRUE;
        }
        if ((int)strlen(e->state) == len && strncmp(e->state, p, len) == 0) {
            take_next = TRUE;
        }
        if (!comma) {
            break;
        }
        p = comma + 1;
    }

    if (!first || (firstlen == (int)strlen(e->state) &&
                   strncmp(e->state, first, firstlen) == 0)) {
        return FALSE;              /* nur eine Art, oder gar keine */
    }
    if (firstlen >= outsize) {
        firstlen = outsize - 1;
    }
    memcpy(out, first, firstlen);
    out[firstlen] = '\0';
    return TRUE;
}

int ha_set_temperature(struct Prefs *p, const char *entity_id, int tenths)
{
    char bodybuf[ID_LEN + 64];
    char num[16];
    char *body = NULL;
    long len = 0;
    int rc;

    temp_text(tenths, num, sizeof(num));
    if (num[0] == '\0') {
        return fail(AH_EHTTP, GetStr(MSG_ERR_BADENTITY));
    }
    sprintf(bodybuf, "{\"entity_id\": \"%s\", \"temperature\": %s}",
            entity_id, num);

    rc = http_request(p, "POST", "/api/services/climate/set_temperature",
                      bodybuf, &body, &len);
    if (body) {
        free(body);
    }
    return rc;
}

int ha_set_hvac_mode(struct Prefs *p, const char *entity_id, const char *mode)
{
    char bodybuf[ID_LEN + 64];
    char *body = NULL;
    long len = 0;
    int rc;

    sprintf(bodybuf, "{\"entity_id\": \"%s\", \"hvac_mode\": \"%s\"}",
            entity_id, mode);

    rc = http_request(p, "POST", "/api/services/climate/set_hvac_mode",
                      bodybuf, &body, &len);
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

/* ------------------------------------------------------------------ */
/* Langzeitstatistik ueber WebSocket                                   */
/* ------------------------------------------------------------------ */

/* Nur so viel WebSocket, wie eine einzige Anfrage braucht: Handshake,
 * Textrahmen hin (maskiert, wie RFC 6455 es vom Client verlangt), Textrahmen
 * zurueck. Kein TLS - wie der Rest des Programms spricht das nur http.
 * Die Antwort ist klein: 30 Tage sind rund 2 KB, 12 Monate unter 1 KB. */

#define WS_MAX_MSG (256L * 1024L)   /* groesser wird eine Antwort nicht */

/* Tage seit 1970 -> Jahr, Monat, Tag. Howard Hinnants civil_from_days,
 * nur mit ganzen Zahlen. */
static void civil_from_days(long z, int *y, int *m, int *d)
{
    long era, doe, yoe, doy, mp;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp  = (5 * doy + 2) / 153;
    *d  = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m  = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y  = (int)(yoe + era * 400 + (*m <= 2 ? 1 : 0));
}

/* Umkehrung: Jahr, Monat, Tag -> Tage seit 1970. */
static long days_from_civil(int y, int m, int d)
{
    long era, yoe, doy, doe;

    y -= (m <= 2) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void stat_date(ULONG start, int *year, int *month, int *day)
{
    civil_from_days((long)((start + 12UL * 3600UL) / 86400UL),
                    year, month, day);
}

/* "Date: Tue, 29 Sep 2026 15:10:57 GMT" aus dem Handshake. Die Uhr des
 * Amiga taugt nicht als Bezug - ein A500 ohne Uhrenkarte glaubt, es sei
 * 1978. Home Assistant weiss es besser. */
static ULONG http_date(const char *hdr)
{
    static const char MON[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *p = strstr(hdr, "\nDate:");
    int d, y, hh, mm, ss, m;
    char mon[4];

    if (!p || sscanf(p + 6, " %*3s, %d %3s %d %d:%d:%d",
                     &d, mon, &y, &hh, &mm, &ss) != 6) {
        return 0;
    }
    mon[3] = '\0';
    for (m = 0; m < 12; m++) {
        if (strncmp(MON + m * 3, mon, 3) == 0) {
            break;
        }
    }
    if (m == 12) {
        return 0;
    }
    return (ULONG)days_from_civil(y, m + 1, d) * 86400UL +
           (ULONG)(hh * 3600L + mm * 60L + ss);
}

static int ws_send_all(int sock, const char *buf, long len)
{
    long sent = 0, n;

    while (sent < len) {
        int w = sock_wait(sock, TRUE, AH_IO_SECS);

        if (w == 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_TIMEOUT));
        }
        if (w < 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_SEND));
        }
        n = send(sock, (APTR)(buf + sent), len - sent, 0);
        if (n < 0 && Errno() == AH_EWOULDBLOCK) {
            continue;
        }
        if (n <= 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_SEND));
        }
        sent += n;
    }
    return AH_OK;
}

static int ws_recv_exact(int sock, char *buf, long len)
{
    long got = 0, n;

    while (got < len) {
        int w = sock_wait(sock, FALSE, AH_IO_SECS);

        if (w == 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_TIMEOUT));
        }
        if (w < 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_RECV));
        }
        n = recv(sock, (APTR)(buf + got), len - got, 0);
        if (n < 0 && Errno() == AH_EWOULDBLOCK) {
            continue;
        }
        if (n <= 0) {
            return fail(AH_ENET, GetStr(MSG_ERR_RECV));
        }
        got += n;
    }
    return AH_OK;
}

/* Ein Textrahmen, maskiert. Die Maske muss nicht geheim sein - sie soll
 * nur Zwischenstationen verwirren, nicht Angreifer. */
static int ws_send_text(int sock, const char *text)
{
    long len = (long)strlen(text);
    char *f = malloc(len + 8);
    long i, h = 0;
    int rc;
    static const UBYTE mask[4] = { 0x37, 0xa5, 0x5c, 0x91 };

    if (!f) {
        return fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
    }
    f[h++] = (char)0x81;                    /* FIN + Text */
    if (len < 126) {
        f[h++] = (char)(0x80 | len);
    } else {
        f[h++] = (char)(0x80 | 126);        /* unsere Anfragen < 64 KB */
        f[h++] = (char)((len >> 8) & 0xff);
        f[h++] = (char)(len & 0xff);
    }
    for (i = 0; i < 4; i++) {
        f[h++] = (char)mask[i];
    }
    for (i = 0; i < len; i++) {
        f[h + i] = (char)(text[i] ^ mask[i & 3]);
    }
    rc = ws_send_all(sock, f, h + len);
    free(f);
    return rc;
}

/* Liest eine ganze Nachricht, auch wenn sie ueber mehrere Rahmen verteilt
 * ist. Pings werden uebergangen - fuer eine Anfrage von wenigen Sekunden
 * lohnt sich keine Antwort darauf. */
static int ws_recv_text(int sock, char **out)
{
    char *buf = NULL;
    long len = 0;
    int rc;

    *out = NULL;
    for (;;) {
        UBYTE h[8];
        ULONG n;
        int op, fin, masked;

        if ((rc = ws_recv_exact(sock, (char *)h, 2)) != AH_OK) {
            break;
        }
        /* Alles aus den ersten zwei Bytes jetzt merken - die erweiterte
         * Laenge wird gleich in denselben Puffer gelesen. */
        fin    = h[0] & 0x80;
        op     = h[0] & 0x0f;
        masked = h[1] & 0x80;
        n      = h[1] & 0x7f;
        if (n == 126) {
            if ((rc = ws_recv_exact(sock, (char *)h, 2)) != AH_OK) {
                break;
            }
            n = ((ULONG)h[0] << 8) | h[1];
        } else if (n == 127) {
            if ((rc = ws_recv_exact(sock, (char *)h, 8)) != AH_OK) {
                break;
            }
            if (h[0] | h[1] | h[2] | h[3]) {
                rc = fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
                break;
            }
            n = ((ULONG)h[4] << 24) | ((ULONG)h[5] << 16) |
                ((ULONG)h[6] << 8) | h[7];
        }
        if (masked) {                       /* Server maskiert nicht */
            rc = fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
            break;
        }
        if (op == 8) {                      /* Server macht zu */
            rc = fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
            break;
        }
        if (len + (long)n > WS_MAX_MSG) {
            rc = fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
            break;
        }
        {
            char *nb = realloc(buf, len + n + 1);

            if (!nb) {
                rc = fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
                break;
            }
            buf = nb;
        }
        if (n && (rc = ws_recv_exact(sock, buf + len, (long)n)) != AH_OK) {
            break;
        }
        if (op == 9 || op == 10) {          /* Ping/Pong: verwerfen */
            continue;
        }
        len += (long)n;
        if (fin) {
            buf[len] = '\0';
            *out = buf;
            return AH_OK;
        }
    }
    free(buf);
    return rc;
}

/* Zahl aus JSON in Hundertsteln, ohne FPU: "97.00000000000023" -> 9700,
 * "-0.5" -> -50, "1.2e-05" -> 0, "null" -> ungueltig. Gerundet wird an der
 * dritten Nachkommastelle. */
static BOOL json_hundredths(const char *s, long *out)
{
    long ip = 0, frac = 0;
    int fd = 0, third = 0, neg = 0, ex = 0, exneg = 0;

    while (*s == ' ') {
        s++;
    }
    if (strncmp(s, "null", 4) == 0) {
        return FALSE;
    }
    if (*s == '-') {
        neg = 1;
        s++;
    }
    if (!isdigit((unsigned char)*s)) {
        return FALSE;
    }
    while (isdigit((unsigned char)*s)) {
        if (ip < 20000000L) {               /* 200000 kWh reichen */
            ip = ip * 10 + (*s - '0');
        }
        s++;
    }
    if (*s == '.') {
        s++;
        while (isdigit((unsigned char)*s)) {
            if (fd < 2) {
                frac = frac * 10 + (*s - '0');
            } else if (fd == 2) {
                third = *s - '0';
            }
            fd++;
            s++;
        }
    }
    while (fd < 2) {
        frac *= 10;
        fd++;
    }
    *out = ip * 100 + frac + (third >= 5 ? 1 : 0);

    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '-' || *s == '+') {
            exneg = (*s == '-');
            s++;
        }
        while (isdigit((unsigned char)*s)) {
            ex = ex * 10 + (*s - '0');
            s++;
        }
        while (ex-- > 0) {
            if (exneg) {
                *out /= 10;
            } else if (*out < 20000000L) {
                *out *= 10;
            }
        }
    }
    if (neg) {
        *out = -*out;
    }
    return TRUE;
}

/* "start" kommt in neueren Home-Assistant-Versionen als Millisekunden, in
 * aelteren als ISO-Zeit. Beides wird verstanden. Millisekunden passen nicht
 * in 32 Bit - also die letzten drei Ziffern einfach abschneiden. */
static ULONG json_start(const char *s)
{
    char dig[24];
    int n = 0;

    while (*s == ' ') {
        s++;
    }
    if (*s == '"') {
        int y, mo, d, hh = 0, mi = 0, ss = 0;

        if (sscanf(s + 1, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) < 3) {
            return 0;
        }
        return (ULONG)days_from_civil(y, mo, d) * 86400UL +
               (ULONG)(hh * 3600L + mi * 60L + ss);
    }
    while (isdigit((unsigned char)*s) && n < (int)sizeof(dig) - 1) {
        dig[n++] = *s++;
    }
    dig[n] = '\0';
    if (n > 10) {
        dig[n - 3] = '\0';                  /* ms -> s */
    }
    return (ULONG)strtoul(dig, NULL, 10);
}

int ha_statistics(struct Prefs *p, const char *entity_id, int period,
                  struct StatPoint *out, int max, int *count)
{
    struct hostent *he;
    struct sockaddr_in sa;
    in_addr_t addr;
    int sock, rc, i, y, m, d;
    char hdr[1024];
    char req[400];
    char *msg = NULL;
    const char *q;
    ULONG now, from;
    long hl = 0;

    *count = 0;
    if (max <= 0) {
        return AH_OK;
    }

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) {
        return fail(AH_ENET, GetStr(MSG_ERR_NOSOCKET));
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)p->port);
    addr = inet_addr((STRPTR)p->host);
    if (addr != INADDR_NONE) {
        sa.sin_addr.s_addr = addr;
    } else {
        he = gethostbyname((UBYTE *)p->host);
        if (!he) {
            CloseLibrary(SocketBase);
            SocketBase = NULL;
            return fail(AH_ENET, GetStr(MSG_ERR_NORESOLVE));
        }
        memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        return fail(AH_ENET, GetStr(MSG_ERR_SOCKET));
    }
    rc = connect_timeout(sock, &sa);
    if (rc != AH_OK) {
        goto done;
    }

    /* Handshake. Der Schluessel ist fest - pruefen muesste ihn nur ein
     * misstrauischer Client, und der sind wir nicht. */
    sprintf(req,
            "GET /api/websocket HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: QW1pSG9tZWFzc2lzdCEhIQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n", p->host, p->port);
    if ((rc = ws_send_all(sock, req, (long)strlen(req))) != AH_OK) {
        goto done;
    }
    /* Kopf bis zur Leerzeile, Byte fuer Byte - danach beginnen die Rahmen,
     * und davon darf nichts verschluckt werden. */
    while (hl < (long)sizeof(hdr) - 1) {
        if ((rc = ws_recv_exact(sock, hdr + hl, 1)) != AH_OK) {
            goto done;
        }
        hl++;
        if (hl >= 4 && memcmp(hdr + hl - 4, "\r\n\r\n", 4) == 0) {
            break;
        }
    }
    hdr[hl] = '\0';
    if (strncmp(hdr, "HTTP/1.1 101", 12) != 0) {
        rc = fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
        goto done;
    }
    now = http_date(hdr);

    /* auth_required -> auth -> auth_ok */
    if ((rc = ws_recv_text(sock, &msg)) != AH_OK) {
        goto done;
    }
    free(msg);
    msg = NULL;
    {
        char *auth = malloc(strlen(p->token) + 64);

        if (!auth) {
            rc = fail(AH_EMEM, GetStr(MSG_ERR_NOMEM));
            goto done;
        }
        sprintf(auth, "{\"type\":\"auth\",\"access_token\":\"%s\"}", p->token);
        rc = ws_send_text(sock, auth);
        free(auth);
        if (rc != AH_OK) {
            goto done;
        }
    }
    if ((rc = ws_recv_text(sock, &msg)) != AH_OK) {
        goto done;
    }
    if (!strstr(msg, "\"auth_ok\"")) {
        rc = fail(AH_EHTTP, GetStr(MSG_ERR_TOKEN401));
        goto done;
    }
    free(msg);
    msg = NULL;

    /* Ab wann: grosszuegig einen Zeitraum mehr als verlangt, uebrig bleiben
     * am Ende die letzten 'max'. Ohne Datum aus dem Handshake ein Jahr
     * zurueck - lieber zu viel als eine leere Antwort. */
    if (now == 0) {
        from = 0;
    } else if (period == AH_PERIOD_MONTH) {
        from = now - (ULONG)(max + 1) * 31UL * 86400UL;
    } else {
        from = now - (ULONG)(max + 1) * 86400UL;
    }
    if (from == 0) {
        y = 2000; m = 1; d = 1;
    } else {
        civil_from_days((long)(from / 86400UL), &y, &m, &d);
    }
    sprintf(req,
            "{\"id\":1,\"type\":\"recorder/statistics_during_period\","
            "\"start_time\":\"%04d-%02d-%02dT00:00:00Z\","
            "\"statistic_ids\":[\"%.96s\"],"
            "\"period\":\"%s\",\"types\":[\"change\"]}",
            y, m, d, entity_id,
            period == AH_PERIOD_MONTH ? "month" : "day");
    if ((rc = ws_send_text(sock, req)) != AH_OK) {
        goto done;
    }
    if ((rc = ws_recv_text(sock, &msg)) != AH_OK) {
        goto done;
    }

    if (!strstr(msg, "\"success\":true")) {
        /* Home Assistant sagt, was nicht passt - auf Englisch, aber
         * genauer als jede eigene Meldung. */
        const char *e = strstr(msg, "\"message\":\"");
        char why[160];

        if (e) {
            e += 11;
            for (i = 0; e[i] && e[i] != '"' && i < (int)sizeof(why) - 1; i++) {
                why[i] = e[i];
            }
            why[i] = '\0';
            rc = fail(AH_EHTTP, why);
        } else {
            rc = fail(AH_EHTTP, GetStr(MSG_ERR_BADRESPONSE));
        }
        goto done;
    }

    /* [{"start":..,"end":..,"change":..}, ...] - die Eintraege der Reihe
     * nach. Mehr als 'max' werden es kaum; wenn doch, rutschen die aeltesten
     * vorne heraus. */
    q = msg;
    while ((q = strstr(q, "\"start\":")) != NULL) {
        const char *next = strstr(q + 8, "\"start\":");
        const char *c = strstr(q, "\"change\":");
        struct StatPoint pt;

        pt.start = json_start(q + 8);
        pt.valid = FALSE;
        pt.value = 0;
        if (c && (!next || c < next)) {
            pt.valid = json_hundredths(c + 9, &pt.value);
        }
        if (*count == max) {
            memmove(out, out + 1, (size_t)(max - 1) * sizeof(*out));
            (*count)--;
        }
        out[(*count)++] = pt;
        q += 8;
    }
    rc = AH_OK;

done:
    free(msg);
    CloseSocket(sock);
    CloseLibrary(SocketBase);
    SocketBase = NULL;
    return rc;
}
