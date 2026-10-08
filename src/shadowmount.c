/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded loopback client for ShadowMountPlus 1.7beta3's documented API.
 * Backport selection is proved from the actual unionfs mount, never guessed
 * from a game folder name or a private cache format. No game data is written.
 */
#include "shadowmount.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#if !defined(TR_HOST_TEST) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/mount.h>
#define SM_NATIVE_STATFS 1
#endif

#define SM_REPLY_MAX (64u * 1024u)
#define SM_HTTP_MAX (SM_REPLY_MAX + 8192u)
#define SM_TIMEOUT_MS 15000
#define SM_IMAGE_ROOT "/mnt/shadowmnt"
#define SM_INFO_ROUTE "/api/v1/games/info"
#define SM_MOUNT_ROUTE "/api/v1/games/mount"
#define SM_UNMOUNT_ROUTE "/api/v1/games/unmount"
#define SM_BODY "{\"title_id\":\"" SM_OVERLAY_TITLE "\"}"
#define SM_RO_BODY "{\"title_id\":\"" SM_OVERLAY_TITLE "\",\"mode\":\"ro\"}"

static int sm_fail(char *err, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(err, SM_OVERLAY_ERR, format, ap);
    va_end(ap);
    return -1;
}

static int copy_path(char *out, const char *in, char *err) {
    if (strlen(in) >= SM_OVERLAY_PATH)
        return sm_fail(err, "ShadowMount yolu cok uzun.");
    strcpy(out, in);
    return 0;
}

static int child_of(const char *path, const char *root) {
    size_t n = strlen(root);
    return !strncmp(path, root, n) && path[n] == '/' && path[n + 1];
}

static int clean_path(const char *p) {
    if (!p || p[0] != '/' || !p[1] || strlen(p) >= SM_OVERLAY_PATH)
        return 0;
    for (const char *s = p + 1; ; ) {
        const char *e = strchr(s, '/');
        size_t n = e ? (size_t)(e - s) : strlen(s);
        if (!n || (n == 1 && s[0] == '.') ||
            (n == 2 && s[0] == '.' && s[1] == '.'))
            return 0;
        for (size_t i = 0; i < n; ++i)
            if ((unsigned char)s[i] < 32 || s[i] == '\\' || s[i] == 127)
                return 0;
        if (!e)
            return 1;
        s = e + 1;
    }
}

static int persistent_path(const char *p) {
    if (!clean_path(p))
        return 0;
    if (child_of(p, "/data") || child_of(p, "/user"))
        return 1;
    return (!strncmp(p, "/mnt/usb", 8) && p[8] >= '0' && p[8] <= '7' &&
            p[9] == '/' && p[10]) ||
           (!strncmp(p, "/mnt/ext", 8) && p[8] >= '0' && p[8] <= '1' &&
            p[9] == '/' && p[10]);
}

static int target_path_ok(const char *p) {
    const char *suffix = "/backports/" SM_OVERLAY_TITLE;
    size_t n = strlen(p), m = strlen(suffix);
    /* /user is allowed for images but never for installer destinations. */
    return persistent_path(p) && !child_of(p, "/user") && n > m &&
           !strcmp(p + n - m, suffix);
}

static int same_file(const sm_overlay_file *a, const sm_overlay_file *b) {
    if (a->kind != b->kind || a->device != b->device || a->inode != b->inode)
        return 0;
    return a->kind == 2 ||
           (a->size == b->size && a->mtime_sec == b->mtime_sec &&
            a->mtime_nsec == b->mtime_nsec && a->ctime_sec == b->ctime_sec &&
            a->ctime_nsec == b->ctime_nsec);
}

static int same_mount(const sm_overlay_mount *a, const sm_overlay_mount *b) {
    return !strcmp(a->point, b->point) && !strcmp(a->from, b->from) &&
           !strcmp(a->type, b->type) && a->fsid[0] == b->fsid[0] &&
           a->fsid[1] == b->fsid[1] && a->read_only == b->read_only;
}

static int native_probe(void *unused, const char *path, sm_overlay_file *out,
                        char *err) {
    (void)unused;
    memset(out, 0, sizeof(*out));
    if (!clean_path(path))
        return sm_fail(err, "Guvenli olmayan ShadowMount yolu.");
    /* Check every existing component, even when the final entry is missing. */
    char walk[SM_OVERLAY_PATH];
    strcpy(walk, path);
    struct stat st;
    for (char *at = walk + 1; ; ++at) {
        if (*at && *at != '/')
            continue;
        char saved = *at;
        *at = 0;
        if (lstat(walk, &st)) {
            int e = errno;
            *at = saved;
            if (e == ENOENT)
                return 0;
            return sm_fail(err, "ShadowMount yolu okunamadi: %s (%s)", path,
                           strerror(e));
        }
        *at = saved;
        if (S_ISLNK(st.st_mode) || (saved && !S_ISDIR(st.st_mode)))
            return sm_fail(err, "ShadowMount yolunda baglanti/normal olmayan dizin var: %s", path);
        if (!saved)
            break;
    }
    char *resolved = realpath(path, NULL);
    if (!resolved)
        return sm_fail(err, "ShadowMount yolu cozumlenemedi: %s", path);
    int canonical = !strcmp(resolved, path);
    free(resolved);
    if (!canonical || (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)))
        return sm_fail(err, "ShadowMount yolu normal dosya/dizin degil: %s", path);
    out->kind = S_ISDIR(st.st_mode) ? 2 : 1;
    out->device = (uint64_t)st.st_dev;
    out->inode = (uint64_t)st.st_ino;
    out->size = st.st_size < 0 ? UINT64_MAX : (uint64_t)st.st_size;
#ifdef __APPLE__
    out->mtime_sec = st.st_mtimespec.tv_sec;
    out->mtime_nsec = st.st_mtimespec.tv_nsec;
    out->ctime_sec = st.st_ctimespec.tv_sec;
    out->ctime_nsec = st.st_ctimespec.tv_nsec;
#else
    out->mtime_sec = st.st_mtim.tv_sec;
    out->mtime_nsec = st.st_mtim.tv_nsec;
    out->ctime_sec = st.st_ctim.tv_sec;
    out->ctime_nsec = st.st_ctim.tv_nsec;
#endif
    return 0;
}

static int native_mount(void *unused, const char *path, sm_overlay_mount *out,
                        char *err) {
    (void)unused;
    memset(out, 0, sizeof(*out));
#ifdef SM_NATIVE_STATFS
    struct statfs s;
    if (statfs(path, &s))
        return sm_fail(err, "ShadowMount baglama bilgisi okunamadi: %s", path);
    if (!memchr(s.f_mntonname, 0, sizeof(s.f_mntonname)) ||
        !memchr(s.f_mntfromname, 0, sizeof(s.f_mntfromname)) ||
        !memchr(s.f_fstypename, 0, sizeof(s.f_fstypename)))
        return sm_fail(err, "ShadowMount statfs metin alanlari sonlandirilmamis.");
    if (copy_path(out->point, s.f_mntonname, err) ||
        copy_path(out->from, s.f_mntfromname, err))
        return -1;
    if (strlen(s.f_fstypename) >= sizeof(out->type))
        return sm_fail(err, "ShadowMount dosya sistemi adi cok uzun.");
    strcpy(out->type, s.f_fstypename);
    _Static_assert(sizeof(s.f_fsid) >= sizeof(out->fsid), "fsid size");
    memcpy(out->fsid, &s.f_fsid, sizeof(out->fsid));
    out->read_only = (s.f_flags & MNT_RDONLY) != 0;
    return 0;
#else
    (void)path;
    return sm_fail(err, "Bu hostta ShadowMount statfs taklidi gerekli.");
#endif
}

/* Beta3 refuses a backport whose directory mtime/ctime is too recent. The
 * read-only settings endpoint omits this setting, so read its local config. */
static int native_wait_stable(void *unused, const char *path, char *err) {
    (void)unused;
    unsigned delay = 10;
    int fd = open("/data/shadowmount/config.ini", O_RDONLY | O_NOFOLLOW);
    if (fd >= 0) {
        struct stat cfg;
        if (fstat(fd, &cfg) || !S_ISREG(cfg.st_mode) || cfg.st_size > 65536) {
            close(fd); return sm_fail(err, "ShadowMount bekleme ayari okunamadi.");
        }
        FILE *file = fdopen(fd, "r");
        if (!file) { close(fd); return sm_fail(err, "ShadowMount ayari acilamadi."); }
        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            if (!strchr(line, '\n') && !feof(file)) {
                fclose(file); return sm_fail(err, "ShadowMount ayar satiri cok uzun.");
            }
            char *key = line;
            while (isspace((unsigned char)*key)) ++key;
            if (*key == '#' || *key == ';') continue;
            char *eq = strchr(key, '=');
            if (!eq) continue;
            *eq = 0;
            char *end = eq;
            while (end > key && isspace((unsigned char)end[-1])) *--end = 0;
            if (strcasecmp(key, "stability_wait_seconds") &&
                strcasecmp(key, "stability_wait_sec")) continue;
            char *value = eq + 1;
            while (isspace((unsigned char)*value)) ++value;
            errno = 0;
            unsigned long parsed = strtoul(value, &end, 10);
            while (isspace((unsigned char)*end)) ++end;
            if (end != value && !*end && !errno && parsed <= 3600) delay = (unsigned)parsed;
        }
        int bad = ferror(file); fclose(file);
        if (bad) return sm_fail(err, "ShadowMount bekleme ayari okunamadi.");
    } else if (errno != ENOENT) {
        return sm_fail(err, "ShadowMount bekleme ayarina erisilemiyor.");
    }
    if (delay >= 60)
        return sm_fail(err, "ShadowMount stability_wait_seconds ayari 60 saniyeden kisa olmali.");
    sm_overlay_file original;
    if (native_probe(NULL, path, &original, err) || original.kind != 2)
        return sm_fail(err, "Yeni backport klasoru okunamadi.");
    for (unsigned waited = 0; waited < 60; ++waited) {
        sm_overlay_file current;
        if (native_probe(NULL, path, &current, err) || !same_file(&original, &current))
            return sm_fail(err, "Bekleme sirasinda backport klasoru degisti.");
        int64_t changed = current.mtime_sec > current.ctime_sec ? current.mtime_sec : current.ctime_sec;
        time_t now = time(NULL);
        if (now != (time_t)-1 && now >= changed && (uint64_t)(now - changed) >= delay)
            return 0;
        struct timespec pause = {1, 0};
        if (nanosleep(&pause, NULL)) return sm_fail(err, "Backport beklemesi kesildi.");
    }
    return sm_fail(err, "Backport klasoru kararlilik beklemesi tamamlanamadi.");
}

static int native_mkdir(void *unused, const char *p, char *err) {
    (void)unused;
    if (mkdir(p, 0777))
        return sm_fail(err, "Backport dizini olusturulamadi: %s (%s)", p, strerror(errno));
    return 0;
}

static int native_rmdir(void *unused, const char *p, char *err) {
    (void)unused;
    if (rmdir(p)) {
        if (errno == ENOTEMPTY || errno == EEXIST) return 1;
        return sm_fail(err, "Bos backport dizini kaldirilamadi: %s (%s)", p, strerror(errno));
    }
    return 0;
}

static int64_t now_ms(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (int64_t)t.tv_sec * 1000 + t.tv_usec / 1000;
}

static int wait_socket(int fd, short events, int64_t until, char *err) {
    for (;;) {
        int64_t left = until - now_ms();
        if (left <= 0)
            return sm_fail(err, "ShadowMount API yanit suresi doldu.");
        struct pollfd p = {fd, events, 0};
        int n = poll(&p, 1, (int)left);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0 || (p.revents & (POLLERR | POLLNVAL)))
            return sm_fail(err, "ShadowMount API baglantisi kesildi/zaman asimi.");
        return 0;
    }
}

static int parse_http(const char *raw, size_t n, char *reply, size_t cap,
                      int *status, char *err) {
    if (memchr(raw, 0, n))
        return sm_fail(err, "ShadowMount HTTP yaniti NUL iceriyor.");
    const char *end = raw + n;
    const char *line = strstr(raw, "\r\n");
    if (!line || line - raw < 12 ||
        (strncmp(raw, "HTTP/1.1 ", 9) && strncmp(raw, "HTTP/1.0 ", 9)) ||
        !isdigit((unsigned char)raw[9]) || !isdigit((unsigned char)raw[10]) ||
        !isdigit((unsigned char)raw[11]) || (raw[12] != ' ' && raw[12] != '\r'))
        return sm_fail(err, "ShadowMount HTTP basligi gecersiz.");
    *status = (raw[9] - '0') * 100 + (raw[10] - '0') * 10 + raw[11] - '0';
    size_t length = 0;
    int have_length = 0;
    const char *p = line + 2;
    while (p < end && strncmp(p, "\r\n", 2)) {
        line = strstr(p, "\r\n");
        if (!line || line - raw > 8192)
            return sm_fail(err, "ShadowMount HTTP basligi tamamlanmadi.");
        const char *colon = memchr(p, ':', (size_t)(line - p));
        if (!colon || p[0] == ' ' || p[0] == '\t')
            return sm_fail(err, "ShadowMount HTTP alanlari gecersiz.");
        size_t key = (size_t)(colon - p);
        if (key == 17 && !strncasecmp(p, "Transfer-Encoding", key))
            return sm_fail(err, "ShadowMount HTTP aktarim turu desteklenmiyor.");
        if (key == 14 && !strncasecmp(p, "Content-Length", key)) {
            if (have_length++)
                return sm_fail(err, "ShadowMount HTTP uzunlugu tekrar ediyor.");
            const char *v = colon + 1;
            while (v < line && (*v == ' ' || *v == '\t')) ++v;
            if (v == line || !isdigit((unsigned char)*v))
                return sm_fail(err, "ShadowMount HTTP uzunlugu gecersiz.");
            while (v < line && isdigit((unsigned char)*v)) {
                unsigned digit = (unsigned)(*v++ - '0');
                if (length > SM_REPLY_MAX / 10u || length * 10u + digit > SM_REPLY_MAX)
                    return sm_fail(err, "ShadowMount API yaniti cok buyuk.");
                length = length * 10u + digit;
            }
            while (v < line && (*v == ' ' || *v == '\t')) ++v;
            if (v != line)
                return sm_fail(err, "ShadowMount HTTP uzunlugu gecersiz.");
        }
        p = line + 2;
    }
    if (p + 2 > end || !have_length)
        return sm_fail(err, "ShadowMount HTTP uzunluk/baslik eksik.");
    p += 2;
    if ((size_t)(end - p) != length || length >= cap)
        return sm_fail(err, "ShadowMount HTTP yaniti eksik veya fazla veri iceriyor.");
    memcpy(reply, p, length);
    reply[length] = 0;
    return 0;
}

#ifdef TR_SHADOWMOUNT_TEST
int sm_overlay_test_http(const char *r, size_t n, char *o, size_t c, int *s, char *e) {
    return parse_http(r, n, o, c, s, e);
}
#endif

static int native_request(void *unused, const char *route, const char *body,
                          char *reply, size_t cap, int *status, char *err) {
    (void)unused;
    int fd = -1, result = -1;
    char request[1024];
    int written = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\nHost: 127.0.0.1:10101\r\n"
        "Content-Type: application/json\r\nContent-Length: %zu\r\n"
        "Connection: close\r\n\r\n%s", route, strlen(body), body);
    if (written < 0 || (size_t)written >= sizeof(request))
        return sm_fail(err, "ShadowMount istegi cok uzun.");
    char *raw = malloc(SM_HTTP_MAX + 1u);
    if (!raw)
        return sm_fail(err, "ShadowMount istemcisi icin bellek yok.");
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        sm_fail(err, "ShadowMount API soketi acilamadi.");
        goto done;
    }
#ifdef SO_NOSIGPIPE
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
#if defined(__APPLE__) || defined(__FreeBSD__)
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_port = htons(10101);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int64_t until = now_ms() + SM_TIMEOUT_MS;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) && errno != EINPROGRESS) {
        sm_fail(err, "ShadowMount yerel API acik degil (127.0.0.1:10101).");
        goto done;
    }
    if (wait_socket(fd, POLLOUT, until, err)) goto done;
    int sockerr = 0;
    socklen_t socklen = sizeof(sockerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &socklen) || sockerr) {
        sm_fail(err, "ShadowMount yerel API baglantisi kurulamadi.");
        goto done;
    }
    size_t sent = 0;
    while (sent < (size_t)written) {
        if (wait_socket(fd, POLLOUT, until, err)) goto done;
        int send_flags = 0;
#ifdef MSG_NOSIGNAL
        send_flags = MSG_NOSIGNAL;
#endif
        ssize_t k = send(fd, request + sent, (size_t)written - sent, send_flags);
        if (k < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (k <= 0) { sm_fail(err, "ShadowMount istegi gonderilemedi."); goto done; }
        sent += (size_t)k;
    }
    size_t used = 0;
    for (;;) {
        if (used == SM_HTTP_MAX) { sm_fail(err, "ShadowMount yaniti siniri asti."); goto done; }
        if (wait_socket(fd, POLLIN, until, err)) goto done;
        ssize_t k = recv(fd, raw + used, SM_HTTP_MAX - used, 0);
        if (k < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (k < 0) { sm_fail(err, "ShadowMount yaniti okunamadi."); goto done; }
        if (!k) break;
        used += (size_t)k;
    }
    raw[used] = 0;
    result = parse_http(raw, used, reply, cap, status, err);
done:
    if (fd >= 0) close(fd);
    free(raw);
    return result;
}

/* Small strict JSON reader: bounded nesting, duplicate recognized fields
 * rejected, JSON escapes decoded before identity/path comparisons. */
typedef struct { const unsigned char *s; size_t n, at; } js;
static void ws(js *r) {
    while (r->at < r->n && strchr(" \r\n\t", r->s[r->at])) ++r->at;
}
static int hex4(js *r, unsigned *u) {
    *u = 0;
    for (int i = 0; i < 4; ++i) {
        if (r->at >= r->n) return -1;
        unsigned c = r->s[r->at++], v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else return -1;
        *u = (*u << 4) | v;
    }
    return 0;
}
static int jstring(js *r, char *out, size_t cap) {
    if (r->at >= r->n || r->s[r->at++] != '"') return -1;
    size_t used = 0;
    while (r->at < r->n) {
        unsigned c = r->s[r->at++];
        if (c == '"') { if (out) out[used] = 0; return 0; }
        if (c < 32) return -1;
        unsigned char encoded[4];
        size_t count = 1;
        encoded[0] = (unsigned char)c;
        if (c == '\\') {
            if (r->at >= r->n) return -1;
            c = r->s[r->at++];
            const char *codes = "\"\\/bfnrt", *values = "\"\\/\b\f\n\r\t";
            const char *p = strchr(codes, (int)c);
            if (p) encoded[0] = (unsigned char)values[p - codes];
            else if (c == 'u') {
                unsigned u;
                if (hex4(r, &u)) return -1;
                if (u >= 0xd800 && u <= 0xdbff) {
                    unsigned low;
                    if (r->at + 2 > r->n || r->s[r->at++] != '\\' ||
                        r->s[r->at++] != 'u' || hex4(r, &low) ||
                        low < 0xdc00 || low > 0xdfff) return -1;
                    u = 0x10000 + ((u - 0xd800) << 10) + low - 0xdc00;
                } else if (u >= 0xdc00 && u <= 0xdfff) return -1;
                if (!u) return -1;
                if (u < 0x80) encoded[0] = (unsigned char)u;
                else if (u < 0x800) {
                    count = 2; encoded[0] = (unsigned char)(0xc0 | (u >> 6));
                    encoded[1] = (unsigned char)(0x80 | (u & 63));
                } else if (u < 0x10000) {
                    count = 3; encoded[0] = (unsigned char)(0xe0 | (u >> 12));
                    encoded[1] = (unsigned char)(0x80 | ((u >> 6) & 63));
                    encoded[2] = (unsigned char)(0x80 | (u & 63));
                } else {
                    count = 4; encoded[0] = (unsigned char)(0xf0 | (u >> 18));
                    encoded[1] = (unsigned char)(0x80 | ((u >> 12) & 63));
                    encoded[2] = (unsigned char)(0x80 | ((u >> 6) & 63));
                    encoded[3] = (unsigned char)(0x80 | (u & 63));
                }
            } else return -1;
        }
        if (out) {
            if (used + count >= cap) return -1;
            memcpy(out + used, encoded, count);
            used += count;
        }
    }
    return -1;
}
static int jvalue(js *r, unsigned depth) {
    ws(r);
    if (depth > 32 || r->at >= r->n) return -1;
    unsigned c = r->s[r->at];
    if (c == '"') return jstring(r, NULL, 0);
    if (c == '{' || c == '[') {
        ++r->at; ws(r);
        unsigned close = c == '{' ? '}' : ']';
        if (r->at < r->n && r->s[r->at] == close) { ++r->at; return 0; }
        for (;;) {
            if (c == '{') {
                if (jstring(r, NULL, 0)) return -1;
                ws(r);
                if (r->at >= r->n || r->s[r->at++] != ':') return -1;
            }
            if (jvalue(r, depth + 1)) return -1;
            ws(r);
            if (r->at >= r->n) return -1;
            if (r->s[r->at] == close) { ++r->at; return 0; }
            if (r->s[r->at++] != ',') return -1;
            ws(r);
        }
    }
    const char *literals[] = {"true", "false", "null"};
    for (size_t i = 0; i < 3; ++i) {
        size_t n = strlen(literals[i]);
        if (n <= r->n - r->at && !memcmp(r->s + r->at, literals[i], n)) {
            r->at += n; return 0;
        }
    }
    if (r->s[r->at] == '-') ++r->at;
    if (r->at >= r->n) return -1;
    if (r->s[r->at] == '0') ++r->at;
    else {
        if (r->s[r->at] < '1' || r->s[r->at] > '9') return -1;
        while (r->at < r->n && isdigit(r->s[r->at])) ++r->at;
    }
    if (r->at < r->n && r->s[r->at] == '.') {
        ++r->at;
        if (r->at == r->n || !isdigit(r->s[r->at])) return -1;
        while (r->at < r->n && isdigit(r->s[r->at])) ++r->at;
    }
    if (r->at < r->n && (r->s[r->at] == 'e' || r->s[r->at] == 'E')) {
        ++r->at;
        if (r->at < r->n && (r->s[r->at] == '+' || r->s[r->at] == '-')) ++r->at;
        if (r->at == r->n || !isdigit(r->s[r->at])) return -1;
        while (r->at < r->n && isdigit(r->s[r->at])) ++r->at;
    }
    return 0;
}

typedef struct {
    char title[SM_OVERLAY_PATH], path[SM_OVERLAY_PATH], runtime[SM_OVERLAY_PATH];
    char source_type[SM_OVERLAY_PATH], mode[SM_OVERLAY_PATH];
    int status, image_backed, managed, mounted, available;
    unsigned seen;
} api_info;

static int parse_json(const char *text, api_info *out, char *err) {
    const char *keys[] = {"status", "title_id", "path", "runtime_path", "source_type",
                         "image_backed", "managed", "mounted", "source_available", "mode"};
    memset(out, 0, sizeof(*out));
    js r = {(const unsigned char *)text, strlen(text), 0};
    ws(&r);
    if (r.at >= r.n || r.s[r.at++] != '{') goto bad;
    ws(&r);
    if (r.at < r.n && r.s[r.at] == '}') goto bad;
    for (;;) {
        char key[128];
        if (jstring(&r, key, sizeof(key))) goto bad;
        ws(&r);
        if (r.at >= r.n || r.s[r.at++] != ':') goto bad;
        ws(&r);
        size_t k;
        for (k = 0; k < sizeof(keys)/sizeof(keys[0]); ++k)
            if (!strcmp(key, keys[k])) break;
        if (k == sizeof(keys)/sizeof(keys[0])) {
            if (jvalue(&r, 1)) goto bad;
        } else {
            if (out->seen & (1u << k)) goto bad;
            out->seen |= 1u << k;
            if (k == 0) {
                if (r.at >= r.n || !isdigit(r.s[r.at])) goto bad;
                unsigned value = 0;
                if (r.s[r.at] == '0' && r.at + 1 < r.n && isdigit(r.s[r.at+1])) goto bad;
                while (r.at < r.n && isdigit(r.s[r.at])) {
                    unsigned digit = r.s[r.at++] - '0';
                    if (value > (unsigned)INT_MAX / 10 || value * 10u + digit > INT_MAX) goto bad;
                    value = value * 10u + digit;
                }
                out->status = (int)value;
            } else if (k >= 5 && k <= 8) {
                int value;
                if (r.n - r.at >= 4 && !memcmp(r.s + r.at, "true", 4)) { value = 1; r.at += 4; }
                else if (r.n - r.at >= 5 && !memcmp(r.s + r.at, "false", 5)) { value = 0; r.at += 5; }
                else goto bad;
                if (k == 5) out->image_backed = value;
                if (k == 6) out->managed = value;
                if (k == 7) out->mounted = value;
                if (k == 8) out->available = value;
            } else {
                char *s = k == 1 ? out->title : k == 2 ? out->path :
                          k == 3 ? out->runtime : k == 4 ? out->source_type : out->mode;
                if (jstring(&r, s, SM_OVERLAY_PATH)) goto bad;
            }
        }
        ws(&r);
        if (r.at >= r.n) goto bad;
        if (r.s[r.at] == '}') { ++r.at; break; }
        if (r.s[r.at++] != ',') goto bad;
        ws(&r);
    }
    ws(&r);
    if (r.at != r.n || !(out->seen & 1u)) goto bad;
    return 0;
bad:
    return sm_fail(err, "ShadowMount JSON alanlari eksik/tekrarli/gecersiz.");
}

static int api(sm_overlay_context *c, const char *route, const char *body,
               api_info *out, char *err) {
    char *reply = calloc(1, SM_REPLY_MAX + 1u);
    if (!reply) return sm_fail(err, "ShadowMount JSON icin bellek yok.");
    int http = 0, result = c->hooks.request(c->hooks.opaque, route, body,
                                          reply, SM_REPLY_MAX + 1u, &http, err);
    if (!result && !memchr(reply, 0, SM_REPLY_MAX + 1u))
        result = sm_fail(err, "ShadowMount yaniti sonlandirilmamis.");
    if (!result) result = parse_json(reply, out, err);
    if (!result && (http != 200 || out->status))
        result = sm_fail(err, "ShadowMount API islemi reddetti (HTTP %d, durum %d).", http, out->status);
    free(reply);
    return result;
}

static int info(sm_overlay_context *c, api_info *out, char *err) {
    if (api(c, SM_INFO_ROUTE, SM_BODY, out, err)) return -1;
    if ((out->seen & 0x1ffu) != 0x1ffu || strcmp(out->title, SM_OVERLAY_TITLE) ||
        strcmp(out->source_type, "image") || !out->image_backed || !out->managed ||
        !out->available || !persistent_path(out->path) || !clean_path(out->runtime) ||
        !child_of(out->runtime, SM_IMAGE_ROOT))
        return sm_fail(err, "ShadowMount dogrulanmis PPSA34015 disk kaynagi bildirmedi.");
    return 0;
}

static int closed(sm_overlay_context *c, char *err) {
    if (!c->hooks.game_closed)
        return sm_fail(err, "ShadowMount oyun-kapali denetimi ayarlanmamis.");
    return c->hooks.game_closed(c->hooks.opaque, err);
}

static int operation(sm_overlay_context *c, int mount, char *err) {
    api_info response;
    if (closed(c, err) || api(c, mount ? SM_MOUNT_ROUTE : SM_UNMOUNT_ROUTE,
                            mount ? SM_RO_BODY : SM_BODY, &response, err)) return -1;
    unsigned mask = (1u << 0) | (1u << 1) | (1u << 7) | (mount ? 1u << 9 : 0);
    if ((response.seen & mask) != mask || strcmp(response.title, SM_OVERLAY_TITLE) ||
        response.mounted != mount || (mount && strcmp(response.mode, "ro")))
        return sm_fail(err, "ShadowMount baglama onayi beklenen oyun/mod ile eslesmedi.");
    return 0;
}

void sm_overlay_init(sm_overlay_context *c, const sm_overlay_hooks *hooks) {
    memset(c, 0, sizeof(*c));
    if (hooks) c->hooks = *hooks;
    if (!c->hooks.request) c->hooks.request = native_request;
    if (!c->hooks.probe) c->hooks.probe = native_probe;
    if (!c->hooks.mount_info) c->hooks.mount_info = native_mount;
    if (!c->hooks.make_directory) c->hooks.make_directory = native_mkdir;
    if (!c->hooks.remove_directory) c->hooks.remove_directory = native_rmdir;
    if (!c->hooks.wait_stable) c->hooks.wait_stable = native_wait_stable;
}

static int capture_mounts(sm_overlay_context *c, char *err) {
    sm_overlay_mount base, title;
    if (c->hooks.mount_info(c->hooks.opaque, c->source, &base, err) ||
        c->hooks.mount_info(c->hooks.opaque, SM_OVERLAY_RUNTIME, &title, err)) return -1;
    /* PS5 unionfs statfs reports the writable upper storage's flags even
     * when the loader requested RO. The base image itself must remain RO;
     * a nullfs-only title view must additionally retain that RO flag. */
    if (!base.read_only || (strcmp(title.type, "unionfs") && !title.read_only) || !clean_path(base.point) ||
        !child_of(base.point, SM_IMAGE_ROOT) ||
        (strcmp(c->source, base.point) && !child_of(c->source, base.point)) ||
        !strcmp(base.type, "unionfs") || !strcmp(base.type, "nullfs") ||
        strcmp(title.point, SM_OVERLAY_RUNTIME))
        return sm_fail(err, "ShadowMount kaynak/baglama salt okunur temel disk degil.");
    if (strcmp(title.type, "unionfs") &&
        (strcmp(title.type, "nullfs") || strcmp(title.from, c->source)))
        return sm_fail(err, "ShadowMount oyun baglamasi kaynakla eslesmiyor.");
    c->source_mount = base;
    c->title_mount = title;
    c->have_mount_snapshot = 1;
    return 0;
}

static int read_selected(sm_overlay_context *c, char *err) {
    c->overlay_proven = !strcmp(c->title_mount.type, "unionfs");
    c->target_missing = 0;
    if (c->overlay_proven) {
        const char *from = c->title_mount.from;
        if (!strncmp(from, "<above>:", 8)) from += 8;
        if (!target_path_ok(from) || copy_path(c->target, from, err))
            return sm_fail(err, "Secilen backport yolu guvenli PPSA34015 dizini degil.");
    } else strcpy(c->target, SM_OVERLAY_FALLBACK);
    if (c->hooks.probe(c->hooks.opaque, c->target, &c->target_identity, err)) return -1;
    if (c->overlay_proven && c->target_identity.kind != 2)
        return sm_fail(err, "Secilen backport normal dizin degil.");
    if (!c->overlay_proven && c->target_identity.kind)
        return sm_fail(err, "Mevcut backport secilmedi; ShadowMount durumu belirsiz.");
    c->target_missing = !c->target_identity.kind;
    return 0;
}

static int image_unchanged(sm_overlay_context *c, api_info *out, char *err) {
    sm_overlay_file image;
    if (info(c, out, err)) return -1;
    if (strcmp(out->path, c->image) || strcmp(out->runtime, c->source))
        return sm_fail(err, "ShadowMount oyun kaynagi islem sirasinda degisti.");
    if (c->hooks.probe(c->hooks.opaque, c->image, &image, err)) return -1;
    if (!same_file(&image, &c->image_identity))
        return sm_fail(err, "Oyun disk dosyasi islem sirasinda degisti.");
    return 0;
}

static int make_fallback(sm_overlay_context *c, char *err) {
    const char *paths[] = {"/data/homebrew", "/data/homebrew/backports", SM_OVERLAY_FALLBACK};
    sm_overlay_file root;
    if (c->hooks.probe(c->hooks.opaque, "/data", &root, err) || root.kind != 2)
        return sm_fail(err, "Dahili /data dizini dogrulanamadi.");
    for (size_t i = 0; i < 3; ++i) {
        sm_overlay_file before;
        if (c->hooks.probe(c->hooks.opaque, paths[i], &before, err)) return -1;
        if (before.kind && (before.kind != 2 || before.device != root.device))
            return sm_fail(err, "Backport ust dizini farkli aygit/normal olmayan dizin.");
        if (before.kind) continue;
        if (closed(c, err) || c->hooks.make_directory(c->hooks.opaque, paths[i], err)) return -1;
        size_t slot = c->created_count;
        strcpy(c->created[slot], paths[i]);
        c->created_count++;
        if (c->hooks.probe(c->hooks.opaque, paths[i], &c->created_identity[slot], err)) return -1;
        if (c->created_identity[slot].kind != 2 || c->created_identity[slot].device != root.device)
            return sm_fail(err, "Yeni backport dizini dogrulanamadi.");
    }
    return 0;
}

static int release_owned(sm_overlay_context *c, char *err) {
    if (!c->owns_mount) return 0;
    if (!c->have_mount_snapshot)
        return sm_fail(err, "Baglama sahipligi kanitlanamadi; salt okunur baglama korundu.");
    api_info current;
    sm_overlay_file source;
    sm_overlay_mount title, base;
    if (closed(c, err) || image_unchanged(c, &current, err)) return -1;
    if (!current.mounted) { c->owns_mount = 0; return 0; }
    if (c->source_identity.kind &&
        (c->hooks.probe(c->hooks.opaque, c->source, &source, err) ||
         !same_file(&source, &c->source_identity)))
        return sm_fail(err, "Temel oyun dizini degisti; baglama korundu.");
    if (c->hooks.mount_info(c->hooks.opaque, SM_OVERLAY_RUNTIME, &title, err) ||
        c->hooks.mount_info(c->hooks.opaque, c->source, &base, err)) return -1;
    if (!same_mount(&title, &c->title_mount) || !same_mount(&base, &c->source_mount))
        return sm_fail(err, "Baglama kimligi degisti; baska baglamaya dokunulmadi.");
    if (operation(c, 0, err)) return -1;
    c->owns_mount = 0;
    c->have_mount_snapshot = 0;
    return 0;
}

int sm_overlay_prepare(sm_overlay_context *c, const char *selector,
                       int check_only, char *err) {
    if (c->prepared || c->image[0] || c->owns_mount || c->created_count)
        return sm_fail(err, "ShadowMount baglami yeniden kullanilamaz.");
    c->check_only = !!check_only;
    api_info first, after;
    if (closed(c, err) || info(c, &first, err)) return -1;
    if (selector && *selector && (!clean_path(selector) || strcmp(selector, first.path)))
        return sm_fail(err, "Secilen disk ShadowMount oyun kaynagi ile eslesmiyor.");
    strcpy(c->source, first.runtime);
    strcpy(c->image, first.path);
    if (c->hooks.probe(c->hooks.opaque, c->image, &c->image_identity, err)) return -1;
    if (c->image_identity.kind != 1 || !c->image_identity.size)
        return sm_fail(err, "Oyun disk kaynagi normal dosya degil.");
    /* The API has no ownership token. Narrow the observation/request race and
     * never claim a mount which appeared since our first observation. */
    if (!first.mounted) {
        if (image_unchanged(c, &after, err)) return -1;
        first.mounted = after.mounted;
    }
    if (first.mounted) {
        /* Do not reconfigure or claim an existing mount. */
        if (capture_mounts(c, err)) return -1;
    } else {
        if (operation(c, 1, err)) return -1;
        c->owns_mount = 1;
        if (capture_mounts(c, err)) return -1;
    }
    if (image_unchanged(c, &after, err) || !after.mounted)
        return sm_fail(err, "ShadowMount baglama sonrasi kaynak dogrulanamadi.");
    if (c->hooks.probe(c->hooks.opaque, c->source, &c->source_identity, err)) return -1;
    if (c->source_identity.kind != 2)
        return sm_fail(err, "Diskin temel oyun dizini okunamadi.");
    if (read_selected(c, err)) return -1;
    if (!c->overlay_proven && !c->check_only) {
        if (!c->owns_mount)
            return sm_fail(err, "Mevcut baglama bize ait degil; yeni overlay icin once ShadowMount'tan oyunu ayirin.");
        if (release_owned(c, err) || make_fallback(c, err) ||
            c->hooks.wait_stable(c->hooks.opaque, SM_OVERLAY_FALLBACK, err) ||
            closed(c, err)) return -1;
        if (image_unchanged(c, &after, err)) return -1;
        if (after.mounted)
            return sm_fail(err, "Yeni baglama baska islem tarafindan acildi; kurulum durduruldu.");
        if (operation(c, 1, err)) return -1;
        c->owns_mount = 1;
        if (capture_mounts(c, err) || image_unchanged(c, &after, err) || !after.mounted)
            return sm_fail(err, "Yeni backport sonrasi disk kaynagi dogrulanamadi.");
        sm_overlay_file source;
        if (c->hooks.probe(c->hooks.opaque, c->source, &source, err)) return -1;
        /* Fresh mount may have a different filesystem id/inode; the physical
         * image is unchanged and source path is revalidated above. */
        if (source.kind != 2) return sm_fail(err, "Yeni temel oyun dizini okunamadi.");
        c->source_identity = source;
        if (read_selected(c, err)) return -1;
        if (!c->overlay_proven || strcmp(c->target, SM_OVERLAY_FALLBACK))
            return sm_fail(err, "Yeni backport secilmedi; kurulum durduruldu.");
    }
    c->prepared = 1;
    return sm_overlay_revalidate(c, err);
}

int sm_overlay_revalidate(sm_overlay_context *c, char *err) {
    if (!c->prepared || !c->have_mount_snapshot)
        return sm_fail(err, "ShadowMount kurulumu hazirlanmamis.");
    if (!c->check_only && (!c->overlay_proven || c->target_missing))
        return sm_fail(err, "Yazma icin secili backport kanitlanamadi.");
    api_info current;
    sm_overlay_file source, target;
    sm_overlay_mount base, title;
    if (closed(c, err) || image_unchanged(c, &current, err)) return -1;
    if (!current.mounted)
        return sm_fail(err, "Oyun disk baglamasi kayboldu.");
    if (c->hooks.probe(c->hooks.opaque, c->source, &source, err) ||
        c->hooks.probe(c->hooks.opaque, c->target, &target, err) ||
        c->hooks.mount_info(c->hooks.opaque, c->source, &base, err) ||
        c->hooks.mount_info(c->hooks.opaque, SM_OVERLAY_RUNTIME, &title, err)) return -1;
    if (!same_file(&source, &c->source_identity) || !same_file(&target, &c->target_identity) ||
        !same_mount(&base, &c->source_mount) || !same_mount(&title, &c->title_mount))
        return sm_fail(err, "Disk/backport/baglama kimligi degisti; kurulum durduruldu.");
    return 0;
}

int sm_overlay_cleanup(sm_overlay_context *c, char *err) {
    if (release_owned(c, err)) return -1;
    if (c->created_count) {
        api_info current;
        if (closed(c, err) || image_unchanged(c, &current, err)) return -1;
        if (current.mounted)
            return sm_fail(err, "Dizinler baska baglamada kullaniliyor olabilir; korundu.");
    }
    while (c->created_count) {
        size_t i = c->created_count - 1;
        sm_overlay_file current;
        if (c->hooks.probe(c->hooks.opaque, c->created[i], &current, err)) return -1;
        if (!same_file(&current, &c->created_identity[i]))
            return sm_fail(err, "Olusturulan dizin degisti; silinmedi: %s", c->created[i]);
        /* Nonempty is expected after successful installation. rmdir never
         * traverses or removes files; all parent directories also remain. */
        int removed = c->hooks.remove_directory(c->hooks.opaque, c->created[i], err);
        if (removed < 0) return -1;
        if (removed > 0) { c->created_count = 0; break; }
        c->created_count--;
    }
    c->prepared = 0;
    return 0;
}
