/* SPDX-License-Identifier: GPL-3.0-or-later
 * FC27 Turkish commentary installer.
 * Notification ABI follows John Tornblom's PS5 SDK hello_world sample.
 * Game/AppInfo ABI follows ShadowMountPlus 1.7beta3.
 * Only the ten embedded audio assets and their AMPR index size fields change.
 */
#include "assets.h"
#include "bundle.h"
#ifndef TR_HOST_TEST
#include "shadowmount.h"
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifndef TR_HOST_TEST
#include <sys/sysctl.h>
#endif

#define TR_PATH 1024
#define TR_ERR 512
#define INDEX_MAX (32u * 1024u * 1024u)
#define PARAM_MAX (1024u * 1024u)
#define CONFIG_PATH "/data/FC27_TR/install.conf"
#define ZIP_BASENAME "FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip"
#define TITLE "PPSA34015"
#define LOG_LIMIT (128u * 1024u)
#define CREATED_DIR_MAX 128
#define DISCOVERY_USB_MAX_DEPTH 4u
#define DISCOVERY_DATA_MAX_DEPTH 32u
#define DISCOVERY_MAX_DIRS 4096u
#define DISCOVERY_MAX_ENTRIES 32768u

typedef struct {
    char game[TR_PATH];
    char zip[TR_PATH];
    char overlay[TR_PATH];
    int check_only;
} options;

typedef struct {
    char path[TR_PATH];
    char stage[TR_PATH];
    char previous[TR_PATH];
    struct stat before;
    int exists;
    int change;
    int stage_owned;
    int backup_owned;
    int committed;
} transaction_file;

static FILE *log_file;
static size_t log_bytes;
static volatile sig_atomic_t cancelled;
static char created_dirs[CREATED_DIR_MAX][TR_PATH];
static size_t created_dir_count;
#ifndef TR_HOST_TEST
static sm_overlay_context image_context;
static int image_context_initialized;
#endif
#ifdef TR_HOST_TEST
static unsigned test_fail_rename;
static unsigned test_rename_count;
#endif

#ifdef TR_OVERLAY_TEST
static void (*test_overlay_validation_hook)(const options *, unsigned);
static void (*test_overlay_final_hash_hook)(const options *);
static unsigned test_overlay_validation_count;
#endif

#ifndef TR_HOST_TEST
typedef struct {
    uint32_t app_id;
    uint64_t unknown1;
    char title_id[14];
    char unknown2[0x3c];
} app_info;
_Static_assert(sizeof(app_info) == 96, "AppInfo ABI size");
_Static_assert(offsetof(app_info, title_id) == 16, "AppInfo title offset");
int sceKernelGetAppInfo(pid_t pid, app_info *info);
typedef struct {
    char unused[45];
    char message[3075];
} notify_request;
int sceKernelSendNotificationRequest(int, notify_request *, size_t, int);
#endif

static int fail(char *err, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(err, TR_ERR, format, ap);
    va_end(ap);
    return -1;
}

static void note(const char *format, ...) {
    char message[TR_ERR];
    va_list ap;
    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    printf("%s\n", message);
    fflush(stdout);
    if (log_file && log_bytes + strlen(message) + 1 < LOG_LIMIT) {
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        log_bytes += strlen(message) + 1;
    }
#ifndef TR_HOST_TEST
    notify_request request;
    memset(&request, 0, sizeof(request));
    snprintf(request.message, sizeof(request.message), "%s", message);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
#endif
}

static void start_log(void) {
#ifndef TR_HOST_TEST
    struct stat st;
    const char *path = "/data/FC27_TR/installer.log";
    if (lstat("/data/FC27_TR", &st) || !S_ISDIR(st.st_mode))
        return;
    if (lstat(path, &st) == 0 && (!S_ISREG(st.st_mode) || st.st_size < 0 ||
                                (uint64_t)st.st_size >= LOG_LIMIT))
        return;
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0666);
    if (fd < 0)
        return;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size >= LOG_LIMIT) {
        close(fd);
        return;
    }
    log_bytes = (size_t)st.st_size;
    log_file = fdopen(fd, "a");
    if (!log_file)
        close(fd);
#endif
}

static void signal_cancel(int signum) {
    (void)signum;
    cancelled = 1;
}

static int path_join(char out[TR_PATH], const char *base, const char *rel,
                     char *err) {
    int n = snprintf(out, TR_PATH, "%s/%s", base, rel);
    if (n < 0 || n >= TR_PATH)
        return fail(err, "Dosya yolu cok uzun.");
    return 0;
}

static int suffix_path(char out[TR_PATH], const char *base, const char *suffix,
                       char *err) {
    int n = snprintf(out, TR_PATH, "%s%s", base, suffix);
    if (n < 0 || n >= TR_PATH)
        return fail(err, "Gecici dosya yolu cok uzun.");
    return 0;
}

static int path_is_relative_safe(const char *p) {
    if (!*p || *p == '/' || strchr(p, '\\'))
        return 0;
    for (const char *start = p; ; ) {
        const char *end = strchr(start, '/');
        size_t n = end ? (size_t)(end - start) : strlen(start);
        if (!n || (n == 1 && start[0] == '.') ||
            (n == 2 && start[0] == '.' && start[1] == '.'))
            return 0;
        if (!end)
            return 1;
        start = end + 1;
    }
}

static int regular_or_missing(const char *path, struct stat *st, int *exists,
                              char *err) {
    if (!lstat(path, st)) {
        if (!S_ISREG(st->st_mode))
            return fail(err, "Normal dosya degil: %s", path);
        *exists = 1;
        return 0;
    }
    if (errno != ENOENT)
        return fail(err, "Dosya bilgisi okunamadi: %s (%s)", path, strerror(errno));
    memset(st, 0, sizeof(*st));
    *exists = 0;
    return 0;
}

static int must_be_missing(const char *path, char *err) {
    struct stat st;
    if (!lstat(path, &st))
        return fail(err, "Onceki islem kalintisi var; silinmedi: %s", path);
    if (errno != ENOENT)
        return fail(err, "Gecici dosya denetimi basarisiz: %s", path);
    return 0;
}

static int read_file(const char *path, size_t maximum, unsigned char **out,
                     size_t *length, struct stat *before, char *err) {
    *out = NULL;
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return fail(err, "Dosya acilamadi: %s (%s)", path, strerror(errno));
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size > maximum) {
        close(fd);
        return fail(err, "Dosya turu veya boyutu uygun degil: %s", path);
    }
    size_t size = (size_t)st.st_size;
    unsigned char *data = malloc(size + 1);
    if (!data) {
        close(fd);
        return fail(err, "Bellek yetersiz.");
    }
    size_t done = 0;
    while (done < size) {
        ssize_t n = read(fd, data + done, size - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            free(data);
            close(fd);
            return fail(err, "Dosya eksik okunuyor: %s", path);
        }
        done += (size_t)n;
    }
    struct stat after;
    if (fstat(fd, &after) || after.st_size != st.st_size ||
        after.st_mtime != st.st_mtime || after.st_ctime != st.st_ctime) {
        free(data);
        close(fd);
        return fail(err, "Okuma sirasinda dosya degisti: %s", path);
    }
    if (close(fd)) {
        free(data);
        return fail(err, "Dosya kapatilamadi: %s", path);
    }
    data[size] = 0;
    *out = data;
    *length = size;
    if (before)
        *before = st;
    return 0;
}

/* Small validating JSON reader. Only top-level string identity fields are used. */
typedef struct {
    const unsigned char *data;
    size_t n, at;
} json_reader;
typedef struct {
    const unsigned char *p;
    size_t n;
    int escaped;
} json_string;

static void json_space(json_reader *r) {
    while (r->at < r->n && (r->data[r->at] == ' ' || r->data[r->at] == '\t' ||
           r->data[r->at] == '\r' || r->data[r->at] == '\n'))
        r->at++;
}

static int json_take_string(json_reader *r, json_string *s) {
    if (r->at >= r->n || r->data[r->at++] != '"')
        return -1;
    size_t start = r->at;
    int escaped = 0;
    while (r->at < r->n) {
        unsigned char c = r->data[r->at++];
        if (c == '"') {
            if (s) {
                s->p = r->data + start;
                s->n = r->at - start - 1;
                s->escaped = escaped;
            }
            return 0;
        }
        if (c < 0x20)
            return -1;
        if (c == '\\') {
            escaped = 1;
            if (r->at == r->n)
                return -1;
            c = r->data[r->at++];
            if (c == 'u') {
                if (r->n - r->at < 4)
                    return -1;
                for (int i = 0; i < 4; ++i)
                    if (!isxdigit((unsigned char)r->data[r->at++]))
                        return -1;
            } else if (!strchr("\"\\/bfnrt", c)) {
                return -1;
            }
        }
    }
    return -1;
}

static int json_value(json_reader *r, unsigned depth) {
    if (depth > 32)
        return -1;
    json_space(r);
    if (r->at == r->n)
        return -1;
    unsigned char c = r->data[r->at];
    if (c == '"')
        return json_take_string(r, NULL);
    if (c == '{' || c == '[') {
        unsigned char close = c == '{' ? '}' : ']';
        r->at++;
        json_space(r);
        if (r->at < r->n && r->data[r->at] == close) {
            r->at++;
            return 0;
        }
        for (;;) {
            if (c == '{') {
                if (json_take_string(r, NULL))
                    return -1;
                json_space(r);
                if (r->at == r->n || r->data[r->at++] != ':')
                    return -1;
            }
            if (json_value(r, depth + 1))
                return -1;
            json_space(r);
            if (r->at == r->n)
                return -1;
            unsigned char sep = r->data[r->at++];
            if (sep == close)
                return 0;
            if (sep != ',')
                return -1;
            json_space(r);
        }
    }
    const char *literal = c == 't' ? "true" : c == 'f' ? "false" :
                          c == 'n' ? "null" : NULL;
    if (literal) {
        size_t size = strlen(literal);
        if (r->n - r->at < size || memcmp(r->data + r->at, literal, size))
            return -1;
        r->at += size;
        return 0;
    }
    if (c == '-') {
        if (++r->at == r->n)
            return -1;
    }
    if (r->data[r->at] == '0')
        r->at++;
    else {
        if (r->data[r->at] < '1' || r->data[r->at] > '9')
            return -1;
        while (r->at < r->n && isdigit(r->data[r->at]))
            r->at++;
    }
    if (r->at < r->n && r->data[r->at] == '.') {
        r->at++;
        if (r->at == r->n || !isdigit(r->data[r->at]))
            return -1;
        while (r->at < r->n && isdigit(r->data[r->at]))
            r->at++;
    }
    if (r->at < r->n && (r->data[r->at] == 'e' || r->data[r->at] == 'E')) {
        r->at++;
        if (r->at < r->n && (r->data[r->at] == '+' || r->data[r->at] == '-'))
            r->at++;
        if (r->at == r->n || !isdigit(r->data[r->at]))
            return -1;
        while (r->at < r->n && isdigit(r->data[r->at]))
            r->at++;
    }
    return 0;
}

static int string_equal(const json_string *s, const char *literal) {
    return !s->escaped && s->n == strlen(literal) &&
           !memcmp(s->p, literal, s->n);
}

static int validate_param(const char *game, int *version003, char *err) {
    char path[TR_PATH];
    if (path_join(path, game, "sce_sys/param.json", err))
        return -1;
    unsigned char *bytes = NULL;
    size_t length;
    if (read_file(path, PARAM_MAX, &bytes, &length, NULL, err))
        return -1;
    json_reader r = {bytes, length, 0};
    if (length >= 3 && !memcmp(bytes, "\xef\xbb\xbf", 3))
        r.at = 3;
    int title_seen = 0, version_seen = 0, good_title = 0, good_version = 0;
    json_space(&r);
    int valid = r.at < r.n && r.data[r.at++] == '{';
    while (valid) {
        json_space(&r);
        if (r.at < r.n && r.data[r.at] == '}') {
            r.at++;
            break;
        }
        json_string key;
        if (json_take_string(&r, &key)) {
            valid = 0;
            break;
        }
        json_space(&r);
        if (r.at == r.n || r.data[r.at++] != ':') {
            valid = 0;
            break;
        }
        json_space(&r);
        if (string_equal(&key, "titleId") || string_equal(&key, "contentVersion")) {
            json_string value;
            if (json_take_string(&r, &value)) {
                valid = 0;
                break;
            }
            if (string_equal(&key, "titleId")) {
                if (title_seen++) {
                    valid = 0;
                    break;
                }
                good_title = string_equal(&value, TITLE);
            } else {
                if (version_seen++) {
                    valid = 0;
                    break;
                }
                *version003 = string_equal(&value, "01.000.003");
                good_version = *version003 || string_equal(&value, "01.000.004");
            }
        } else if (json_value(&r, 1)) {
            valid = 0;
            break;
        }
        json_space(&r);
        if (r.at == r.n) {
            valid = 0;
            break;
        }
        unsigned char sep = r.data[r.at++];
        if (sep == '}')
            break;
        if (sep != ',') {
            valid = 0;
            break;
        }
        json_space(&r);
        if (r.at == r.n || r.data[r.at] == '}') {
            valid = 0;
            break;
        }
    }
    json_space(&r);
    valid = valid && r.at == r.n && title_seen == 1 && version_seen == 1;
    free(bytes);
    if (!valid)
        return fail(err, "param.json bozuk veya kimlik alanlari tekrar ediyor.");
    if (!good_title || !good_version)
        return fail(err, "Hedef PPSA34015, surum 01.000.003 veya 01.000.004 olmali.");
    return 0;
}

static uint32_t little32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}
static uint64_t little64(const unsigned char *p) {
    return (uint64_t)little32(p) | (uint64_t)little32(p + 4) << 32;
}
static void store64(unsigned char *p, uint64_t n) {
    for (int i = 0; i < 8; ++i) {
        p[i] = (unsigned char)n;
        n >>= 8;
    }
}
static int ascii_ci_compare(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++), cb = tolower((unsigned char)*b++);
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }
    return *a ? 1 : *b ? -1 : 0;
}
static int sorted_path_compare(const void *a, const void *b) {
    return ascii_ci_compare(*(char *const *)a, *(char *const *)b);
}

static int patch_index(const unsigned char *original, size_t size,
                       unsigned char **patched, unsigned *changes, char *err) {
    *patched = NULL;
    *changes = 0;
    if (size < 48 || memcmp(original, "AMPRIDX3", 8) ||
        little32(original + 8) != 3 || little32(original + 12) != 24)
        return fail(err, "AMPRIDX3 / surum 3 / kayit 24 indeksi gerekli.");
    uint64_t count = little64(original + 16), paths = little64(original + 24);
    uint64_t hash_offset = little64(original + 32);
    uint64_t slot_size = little32(original + 40), slots = little32(original + 44);
    uint64_t blob_start = 48 + count * 24;
    if (!count || count > 100000 || !paths || blob_start > size || paths > size - blob_start ||
        slot_size != 16 || !slots || (slots & (slots - 1)) || slots < count ||
        hash_offset % 8 || hash_offset < blob_start + paths || hash_offset > size ||
        slot_size * slots != size - hash_offset)
        return fail(err, "AMPR indeks sinirlari tutarsiz.");
    char **all_paths = calloc((size_t)count, sizeof(*all_paths));
    unsigned char *output = malloc(size);
    if (!all_paths || !output) {
        free(all_paths);
        free(output);
        return fail(err, "Indeks icin bellek yetersiz.");
    }
    memcpy(output, original, size);
    size_t offsets[TR_ASSET_COUNT];
    for (int i = 0; i < TR_ASSET_COUNT; ++i)
        offsets[i] = 0;
    int result = 0;
    for (uint64_t i = 0; i < count; ++i) {
        size_t offset = 48 + (size_t)i * 24;
        uint64_t po = little32(original + offset), pl = little32(original + offset + 4);
        if (!pl || pl >= TR_PATH || po > paths || pl > paths - po) {
            result = fail(err, "Indeksteki dosya yolu sinir disi.");
            break;
        }
        const unsigned char *p = original + (size_t)blob_start + (size_t)po;
        all_paths[i] = malloc((size_t)pl + 1);
        if (!all_paths[i]) {
            result = fail(err, "Indeks yolu icin bellek yetersiz.");
            break;
        }
        for (uint64_t j = 0; j < pl; ++j) {
            if (p[j] < 0x20 || p[j] > 0x7e) {
                result = fail(err, "Indekste desteklenmeyen dosya yolu var.");
                break;
            }
        }
        if (result)
            break;
        memcpy(all_paths[i], p, (size_t)pl);
        all_paths[i][pl] = 0;
        for (int j = 0; j < TR_ASSET_COUNT; ++j) {
            char expected[TR_PATH];
            int n = snprintf(expected, sizeof(expected), "/app0/%s", tr_assets[j].path);
            if (n < 0 || (size_t)n >= sizeof(expected)) {
                result = fail(err, "Paket yolu cok uzun.");
                break;
            }
            if (!ascii_ci_compare(all_paths[i], expected)) {
                if (offsets[j]) {
                    result = fail(err, "Tekrar eden Turkce indeks yolu.");
                    break;
                }
                uint64_t old = little64(original + offset + 8);
                if (old && old != tr_assets[j].size) {
                    result = fail(err, "Turkce indeks boyutu bu paketle uyusmuyor: %s", tr_assets[j].path);
                    break;
                }
                offsets[j] = offset;
                if (old != tr_assets[j].size) {
                    store64(output + offset + 8, tr_assets[j].size);
                    (*changes)++;
                }
            }
        }
        if (result)
            break;
    }
    if (!result) {
        qsort(all_paths, (size_t)count, sizeof(*all_paths), sorted_path_compare);
        for (uint64_t i = 1; i < count; ++i)
            if (!ascii_ci_compare(all_paths[i - 1], all_paths[i])) {
                result = fail(err, "Tekrar eden indeks yolu; islem yapilmadi.");
                break;
            }
    }
    if (!result)
        for (int i = 0; i < TR_ASSET_COUNT; ++i)
            if (!offsets[i]) {
                result = fail(err, "Gerekli Turkce indeks yolu yok: %s", tr_assets[i].path);
                break;
            }
    for (uint64_t i = 0; i < count; ++i)
        free(all_paths[i]);
    free(all_paths);
    if (result) {
        free(output);
        return result;
    }
    *patched = output;
    return 0;
}

static int game_is_closed(char *err) {
#ifdef TR_HOST_TEST
    (void)err;
    return 1;
#else
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    size_t size = 0;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) || !size || size > 64u * 1024u * 1024u)
        return fail(err, "Calisan oyunlar listelenemedi; kurulum durduruldu.");
    size_t capacity = size + size / 4 + 4096;
    unsigned char *records = malloc(capacity);
    if (!records)
        return fail(err, "Islem listesi icin bellek yetersiz.");
    size = capacity;
    if (sysctl(mib, 4, records, &size, NULL, 0) || size > capacity || !size) {
        free(records);
        return fail(err, "Calisan oyunlar okunamadi; kurulum durduruldu.");
    }
    size_t offset = 0;
    int closed = 1;
    while (offset < size) {
        int32_t record_size, process_id;
        if (size - offset < 76) {
            closed = fail(err, "Islem listesi bozuk; kurulum durduruldu.");
            break;
        }
        memcpy(&record_size, records + offset, sizeof(record_size));
        if (record_size < 76 || (size_t)record_size > size - offset) {
            closed = fail(err, "Islem kaydi sinir disi; kurulum durduruldu.");
            break;
        }
        memcpy(&process_id, records + offset + 72, sizeof(process_id));
        if (process_id > 0 && process_id != getpid()) {
            app_info info;
            memset(&info, 0, sizeof(info));
            if (!sceKernelGetAppInfo((pid_t)process_id, &info) &&
                !memcmp(info.title_id, TITLE, sizeof(TITLE))) {
                closed = 0;
                break;
            }
        }
        offset += (size_t)record_size;
    }
    free(records);
    return closed;
#endif
}

static int require_closed(char *err) {
    int state = game_is_closed(err);
    if (state < 0)
        return -1;
    if (!state)
        return fail(err, "FC27 acik. Oyunu tamamen kapatip yeniden deneyin.");
    if (cancelled)
        return fail(err, "Kurulum iptal edildi.");
    return 0;
}

static int copy_path(char out[TR_PATH], const char *value, char *err) {
    if (value[0] != '/' || strlen(value) >= TR_PATH)
        return fail(err, "Ayar dosyasinda tam ve kisa bir yol gerekli.");
    strcpy(out, value);
    return 0;
}

#ifndef TR_HOST_TEST
static int load_config(options *opt, char *err) {
    struct stat st;
    if (lstat(CONFIG_PATH, &st)) {
        if (errno == ENOENT)
            return 0;
        return fail(err, "install.conf okunamadi.");
    }
    unsigned char *data;
    size_t length;
    if (read_file(CONFIG_PATH, 8192, &data, &length, NULL, err))
        return -1;
    int seen_game = 0, seen_zip = 0, seen_overlay = 0, seen_mode = 0, result = 0;
    if (memchr(data, 0, length)) {
        free(data);
        return fail(err, "install.conf gecersiz.");
    }
    char *save = NULL;
    for (char *line = strtok_r((char *)data, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        while (isspace((unsigned char)*line))
            line++;
        size_t n = strlen(line);
        while (n && isspace((unsigned char)line[n - 1]))
            line[--n] = 0;
        if (!*line || *line == '#')
            continue;
        char *equal = strchr(line, '=');
        if (!equal) {
            result = fail(err, "install.conf satiri key=value olmali.");
            break;
        }
        *equal++ = 0;
        if (!strcmp(line, "game") && !seen_game++)
            result = copy_path(opt->game, equal, err);
        else if (!strcmp(line, "zip") && !seen_zip++)
            result = copy_path(opt->zip, equal, err);
        else if (!strcmp(line, "overlay") && !seen_overlay++)
            result = copy_path(opt->overlay, equal, err);
        else if (!strcmp(line, "mode") && !seen_mode++) {
            if (!strcmp(equal, "check"))
                opt->check_only = 1;
            else if (!strcmp(equal, "install"))
                opt->check_only = 0;
            else
                result = fail(err, "mode=check veya mode=install gerekli.");
        } else
            result = fail(err, "Tekrar eden veya bilinmeyen install.conf ayari.");
        if (result)
            break;
    }
    free(data);
    return result;
}
#endif

static int discover_zip(char out[TR_PATH], char *err) {
    const char *internal[] = {"/data/FC27_TR", "/data"};
    struct stat st;
    for (size_t i = 0; i < sizeof(internal) / sizeof(internal[0]); ++i) {
        if (path_join(out, internal[i], ZIP_BASENAME, err))
            return -1;
        if (!lstat(out, &st) && S_ISREG(st.st_mode))
            return 0;
    }
    for (int usb = 0; usb < 8; ++usb) {
        for (int sub = 0; sub < 2; ++sub) {
            char base[TR_PATH];
            snprintf(base, sizeof(base), "/mnt/usb%d%s", usb, sub ? "" : "/FC27_TR");
            if (path_join(out, base, ZIP_BASENAME, err))
                return -1;
            if (!lstat(out, &st) && S_ISREG(st.st_mode))
                return 0;
        }
    }
    out[0] = 0;
    return fail(err, "Turkce ZIP yok. /data/FC27_TR veya USB/FC27_TR klasorune koyun.");
}

/* Search metadata, never game content. These limits bound work on mixed-use SSDs. */
typedef struct {
    size_t directories;
    size_t entries;
    size_t max_directories;
    size_t max_entries;
    unsigned max_depth;
    dev_t device;
    char *found;
} game_search;

static int game_layout(const char *game, dev_t device, char *err) {
    static const struct {
        const char *name;
        int directory;
    } required[] = {
        {"sce_sys", 1}, {"sce_sys/param.json", 0},
        {"Data", 1}, {"Data/Ps5", 1}, {"ampr_emu.index", 0}
    };
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        char path[TR_PATH];
        struct stat st;
        if (path_join(path, game, required[i].name, err))
            return -1;
        if (lstat(path, &st) || st.st_dev != device ||
            (required[i].directory ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode)))
            return fail(err, "Oyun klasorunde normal %s bulunamadi.", required[i].name);
    }
    return 0;
}

static int ignored_search_directory(const char *name) {
    return name[0] == '.' || !strcasecmp(name, "System Volume Information") ||
           !strcasecmp(name, "$RECYCLE.BIN");
}

/* 1 means an app metadata boundary; do not descend into any game's Data tree. */
static int inspect_game_directory(const char *path, game_search *search, char *err) {
    char metadata[TR_PATH], identity[TR_PATH];
    struct stat st;
    if (path_join(metadata, path, "sce_sys", err) ||
        path_join(identity, path, "sce_sys/param.json", err))
        return -1;
    if (lstat(metadata, &st)) {
        if (errno == ENOENT || errno == ENOTDIR)
            return 0;
        return fail(err, "Oyun aramasinda klasor okunamadi: %s", metadata);
    }
    /* Do not traverse a redirected or separately mounted metadata directory. */
    if (!S_ISDIR(st.st_mode) || st.st_dev != search->device)
        return 1;
    if (lstat(identity, &st)) {
        if (errno == ENOENT || errno == ENOTDIR)
            return 0;
        return fail(err, "Oyun kimligi okunamadi: %s", identity);
    }
    if (!S_ISREG(st.st_mode) || st.st_dev != search->device)
        return 1;
    int version003 = 0;
    char candidate_error[TR_ERR];
    if (validate_param(path, &version003, candidate_error) ||
        game_layout(path, search->device, candidate_error))
        return 1;
    char *canonical = realpath(path, NULL);
    if (!canonical)
        return fail(err, "Oyun klasoru yolu cozumlenemedi: %s", path);
    if (strlen(canonical) >= TR_PATH) {
        free(canonical);
        return fail(err, "Oyun klasoru yolu cok uzun.");
    }
    if (search->found[0] && strcmp(search->found, canonical)) {
        free(canonical);
        return fail(err, "Birden fazla uygun PPSA34015 klasoru var. install.conf ile game= yolunu secin.");
    }
    strcpy(search->found, canonical);
    free(canonical);
    return 1;
}

static int walk_game_directories(const char *path, unsigned depth,
                                 game_search *search, char *err) {
    if (cancelled)
        return fail(err, "Oyun aramasi iptal edildi.");
    if (++search->directories > search->max_directories)
        return fail(err, "Oyun arama sinirina ulasildi. install.conf ile game= yolunu belirtin.");
    int candidate = inspect_game_directory(path, search, err);
    if (candidate < 0)
        return -1;
    if (candidate || depth == search->max_depth)
        return 0;
    DIR *dir = opendir(path);
    if (!dir)
        return fail(err, "Oyun aramasinda klasor acilamadi: %s. install.conf ile game= belirtin.", path);
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) {
            if (errno)
                result = fail(err, "Oyun aramasinda klasor okunamadi: %s", path);
            break;
        }
        if (++search->entries > search->max_entries) {
            result = fail(err, "Oyun arama sinirina ulasildi. install.conf ile game= yolunu belirtin.");
            break;
        }
        if (ignored_search_directory(entry->d_name))
            continue;
        char child[TR_PATH];
        struct stat st;
        if (path_join(child, path, entry->d_name, err)) {
            result = -1;
            break;
        }
        if (lstat(child, &st)) {
            result = fail(err, "Oyun aramasinda yol okunamadi: %s", child);
            break;
        }
        if (!S_ISDIR(st.st_mode) || st.st_dev != search->device)
            continue;
        if (walk_game_directories(child, depth + 1, search, err)) {
            result = -1;
            break;
        }
    }
    if (closedir(dir) && !result)
        result = fail(err, "Oyun aramasinda klasor kapatilamadi: %s", path);
    return result;
}

#ifdef TR_DISCOVERY_TEST
/* Test-only prefix redirects the real /data and /mnt/usbN root selection
   into a temporary host fixture. It is absent from the released installer. */
static const char *discovery_test_prefix;
#endif

static int scan_game_root(const char *root, unsigned max_depth,
                           char found[TR_PATH], char *err) {
#ifdef TR_DISCOVERY_TEST
    char mapped[TR_PATH];
    if (discovery_test_prefix) {
        if (path_join(mapped, discovery_test_prefix, root + 1, err))
            return -1;
        root = mapped;
    }
#endif
    struct stat st;
    if (lstat(root, &st)) {
        if (errno == ENOENT || errno == ENOTDIR)
            return 0;
        return fail(err, "Oyun arama kokune erisilemiyor: %s", root);
    }
    if (!S_ISDIR(st.st_mode))
        return 0;
    /* All production roots are absolute, normalized paths. Reject redirected
       parents too, rather than following an etaHEN/games symlink elsewhere. */
    char *canonical = realpath(root, NULL);
    if (!canonical)
        return fail(err, "Oyun arama koku cozumlenemedi: %s", root);
    int redirected = strcmp(root, canonical) != 0;
    free(canonical);
    if (redirected)
        return 0;
    game_search search = {0, 0, DISCOVERY_MAX_DIRS, DISCOVERY_MAX_ENTRIES,
                          max_depth, st.st_dev, found};
    return walk_game_directories(root, 0, &search, err);
}

static int discover_game_candidate(char out[TR_PATH], char *err) {
    out[0] = 0;
    if (scan_game_root("/data", DISCOVERY_DATA_MAX_DEPTH, out, err))
        return -1;
    for (int usb = 0; usb < 8; ++usb) {
        char root[TR_PATH];
        snprintf(root, sizeof(root), "/mnt/usb%d", usb);
        if (scan_game_root(root, DISCOVERY_USB_MAX_DEPTH, out, err))
            return -1;
    }
    return 0;
}

static int discover_game(char out[TR_PATH], char *err) {
    if (discover_game_candidate(out, err))
        return -1;
    if (!out[0])
        return fail(err, "Uygun PPSA34015 oyun klasoru bulunamadi. Klasor konumunu ve param.json/indeks dosyalarini kontrol edin; install.conf ile game= belirtilebilir.");
    return 0;
}

#ifndef TR_HOST_TEST
static int image_game_closed(void *unused, char *err) {
    (void)unused;
    return require_closed(err);
}

/* Folder discovery must finish successfully before automatic image fallback.
 * A partial or ambiguous scan is never silently treated as an absent game. */
static int prepare_native_target(options *opt, char *err) {
    char selector[TR_PATH] = {0};
    if (!opt->game[0]) {
        if (discover_game_candidate(opt->game, err))
            return -1;
        if (opt->game[0]) {
            if (opt->overlay[0])
                return fail(err, "Klasor oyunda overlay= kullanilamaz; bu ayari kaldirin.");
            return 0;
        }
    } else {
        struct stat st;
        if (lstat(opt->game, &st))
            return fail(err, "Secilen oyun yolu okunamadi: %s", opt->game);
        if (S_ISDIR(st.st_mode) && !opt->overlay[0])
            return 0;
        if (!S_ISREG(st.st_mode))
            return fail(err, "Goruntu icin game= fiziksel oyun disk dosyasini gostermeli.");
        strcpy(selector, opt->game);
    }
    sm_overlay_hooks hooks = {0};
    hooks.game_closed = image_game_closed;
    sm_overlay_init(&image_context, &hooks);
    image_context_initialized = 1;
    note("FC27 TR: ShadowMount oyun goruntusu kontrol ediliyor.");
    if (sm_overlay_prepare(&image_context, selector[0] ? selector : NULL,
                           opt->check_only, err))
        return -1;
    if (opt->overlay[0] && strcmp(opt->overlay, image_context.target))
        return fail(err, "overlay= ShadowMount'un sectigi klasorle uyusmuyor.");
    if (copy_path(opt->game, image_context.source, err) ||
        copy_path(opt->overlay, image_context.target, err))
        return -1;
    if (opt->check_only && image_context.target_missing)
        note("FC27 TR kontrol: yeni ses klasoru kurulumda olusturulacak. Kontrolde klasor acilmadi.");
    return 0;
}

static int cleanup_native_target(void) {
    if (!image_context_initialized)
        return 0;
    char cleanup_error[TR_ERR] = {0};
    int result = sm_overlay_cleanup(&image_context, cleanup_error);
    image_context_initialized = 0;
    if (result)
        note("FC27 TR baglama temizligi: %s", cleanup_error);
    return result;
}
#endif

#if !defined(TR_HOST_TEST) || defined(TR_DISCOVERY_TEST)
/* Called after realpath, so component traversal has already been normalized. */
static int game_storage_root(const char *canonical, char root[32]) {
    if (!strncmp(canonical, "/data", 5) &&
        (canonical[5] == '/' || canonical[5] == 0)) {
        strcpy(root, "/data");
        return 1;
    }
    if (!strncmp(canonical, "/mnt/usb", 8) &&
        canonical[8] >= '0' && canonical[8] <= '7' &&
        (canonical[9] == '/' || canonical[9] == 0)) {
        snprintf(root, 32, "/mnt/usb%c", canonical[8]);
        return 1;
    }
    root[0] = 0;
    return 0;
}
#endif

static int canonical_game_path(char path[TR_PATH], int mounted_source, char *err) {
    char trimmed[TR_PATH];
    strcpy(trimmed, path);
    size_t path_length = strlen(trimmed);
    while (path_length > 1 && trimmed[path_length - 1] == '/')
        trimmed[--path_length] = 0;
    struct stat original;
    if (lstat(trimmed, &original) || !S_ISDIR(original.st_mode))
        return fail(err, "Hedef normal bir oyun klasoru olmali: %s", path);
    char *canonical = realpath(path, NULL);
    if (!canonical)
        return fail(err, "Oyun klasoru acilamadi: %s", path);
    if (strlen(canonical) >= TR_PATH) {
        free(canonical);
        return fail(err, "Oyun klasoru yolu cok uzun.");
    }
    struct stat st;
    int ok = !lstat(canonical, &st) && S_ISDIR(st.st_mode);
#ifndef TR_HOST_TEST
    /* Image mount points are not writable source folders. Only normal /data
       or USB storage is supported; do not redirect installs into /app0 etc. */
    char storage[32];
    struct stat storage_st;
    ok = ok && (mounted_source || (game_storage_root(canonical, storage) &&
         !stat(storage, &storage_st) && st.st_dev == storage_st.st_dev));
#else
    (void)mounted_source;
#endif
    if (!ok) {
        free(canonical);
        return fail(err, "Hedef /data veya USB uzerinde normal oyun klasoru olmali; bagli oyun goruntusu kullanilamaz.");
    }
    if (game_layout(canonical, st.st_dev, err)) {
        free(canonical);
        return -1;
    }
    strcpy(path, canonical);
    free(canonical);
    return 0;
}

static int canonical_game(char path[TR_PATH], char *err) {
    return canonical_game_path(path, 0, err);
}

/* Native integration boundary. The resolver must authenticate this exact
 * mounted source and the loader-selected writable overlay, without mounting
 * or writing anything here. This guard is checked again before/after commit.
 * Configuration alone must never authorize an arbitrary mounted source. */
static int verify_overlay_source(const options *opt, char *err) {
#ifdef TR_HOST_TEST
#ifdef TR_OVERLAY_TEST
    if (test_overlay_validation_hook)
        test_overlay_validation_hook(opt, ++test_overlay_validation_count);
#endif
    (void)opt;
    (void)err;
    return 0;
#else
    if (!image_context_initialized || strcmp(opt->game, image_context.source) ||
        strcmp(opt->overlay, image_context.target))
        return fail(err, "Goruntu kurulumu dogrulanmis ShadowMount kaynagi gerektirir.");
    return sm_overlay_revalidate(&image_context, err);
#endif
}

static int reject_asset_pack(const char *game, char *err) {
    char path[TR_PATH];
    struct stat st;
    if (path_join(path, game, "ampr_assets.index", err))
        return -1;
    if (!lstat(path, &st))
        return fail(err, "AMPR varlik paketi (ampr_assets.index) bulunan oyunda overlay kurulumu desteklenmiyor.");
    if (errno != ENOENT)
        return fail(err, "AMPR varlik paketi denetlenemedi: %s", path);
    return 0;
}

static int path_contains(const char *parent, const char *child) {
    size_t n = strlen(parent);
    return !strncmp(parent, child, n) && (child[n] == 0 || child[n] == '/');
}

/* Resolve an existing parent without creating the overlay in check mode. */
static int prepare_overlay_path(options *opt, char anchor[TR_PATH],
                                struct stat *anchor_stat, char *err) {
    size_t length = strlen(opt->overlay);
    while (length > 1 && opt->overlay[length - 1] == '/')
        opt->overlay[--length] = 0;
    if (opt->overlay[0] != '/' || !path_is_relative_safe(opt->overlay + 1))
        return fail(err, "Overlay icin normal tam klasor yolu gerekli.");
    if (path_contains(opt->game, opt->overlay) || path_contains(opt->overlay, opt->game))
        return fail(err, "Overlay ve kaynak oyun klasorleri birbirinden ayri olmali.");
    strcpy(anchor, opt->overlay);
    for (;;) {
        if (!lstat(anchor, anchor_stat))
            break;
        if (errno != ENOENT)
            return fail(err, "Overlay ust klasoru okunamadi: %s", anchor);
        char *slash = strrchr(anchor, '/');
        if (!slash || slash == anchor)
            return fail(err, "Overlay icin mevcut bir depolama klasoru gerekli.");
        *slash = 0;
    }
    if (!S_ISDIR(anchor_stat->st_mode))
        return fail(err, "Overlay ust yolu normal klasor degil.");
    char *canonical = realpath(anchor, NULL);
    int same = canonical && !strcmp(canonical, anchor);
    free(canonical);
    if (!same)
        return fail(err, "Overlay yolunda sembolik baglanti kullanilamaz.");
#ifndef TR_HOST_TEST
    char storage[32];
    struct stat storage_stat;
    if (!game_storage_root(opt->overlay, storage) || stat(storage, &storage_stat) ||
        storage_stat.st_dev != anchor_stat->st_dev)
        return fail(err, "Overlay normal /data veya USB depolamasinda olmali.");
#endif
    return 0;
}

/* Reject symlink parents. Missing parents are created only after the game guard. */
static int parent_dirs(const char *game, const char *path, int create, char *err) {
    size_t base_size = strlen(game);
    if (strncmp(path, game, base_size) || path[base_size] != '/')
        return fail(err, "Paket yolu oyun disina cikiyor.");
    char copy[TR_PATH];
    strcpy(copy, path);
    for (char *p = copy + base_size + 1; *p; ++p) {
        if (*p != '/')
            continue;
        *p = 0;
        struct stat st;
        if (!lstat(copy, &st)) {
            if (!S_ISDIR(st.st_mode)) {
                *p = '/';
                return fail(err, "Ust klasor normal dizin degil: %s", copy);
            }
        } else if (errno != ENOENT) {
            *p = '/';
            return fail(err, "Ust klasor bilgisi okunamadi: %s", copy);
        } else if (create) {
            if (created_dir_count == CREATED_DIR_MAX || mkdir(copy, 0777)) {
                *p = '/';
                return fail(err, "Turkce dosya klasoru olusturulamadi (%s).", strerror(errno));
            }
            strcpy(created_dirs[created_dir_count++], copy);
        }
        *p = '/';
    }
    return 0;
}

static int prepare_transaction_file(transaction_file *file, const char *game,
                                    const char *relative, char *err) {
    memset(file, 0, sizeof(*file));
    if (!path_is_relative_safe(relative) || path_join(file->path, game, relative, err) ||
        suffix_path(file->stage, file->path, ".tr-stage", err) ||
        suffix_path(file->previous, file->path, ".tr-previous", err))
        return fail(err, "Guvenli paket yolu olusturulamadi.");
    if (parent_dirs(game, file->path, 0, err) ||
        regular_or_missing(file->path, &file->before, &file->exists, err) ||
        must_be_missing(file->stage, err) || must_be_missing(file->previous, err))
        return -1;
    return 0;
}

static int stat_same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_size == b->st_size && a->st_mtime == b->st_mtime &&
           a->st_ctime == b->st_ctime && a->st_mode == b->st_mode;
}

static int original_unchanged(const transaction_file *file, char *err) {
    struct stat now;
    int exists;
    if (regular_or_missing(file->path, &now, &exists, err))
        return -1;
    if (exists != file->exists || (exists && !stat_same(&now, &file->before)))
        return fail(err, "Kurulum sirasinda hedef degisti: %s", file->path);
    return 0;
}

static int finish_stage(int fd, const transaction_file *file, char *err) {
    mode_t mode = file->exists ? file->before.st_mode & 07777 : 0666;
    if (fchmod(fd, mode))
        return fail(err, "Gecici dosyanin izinleri ayarlanamadi: %s", file->path);
    if (file->exists) {
        struct timespec times[2];
#ifdef __APPLE__
        times[0] = file->before.st_atimespec;
        times[1] = file->before.st_mtimespec;
#else
        times[0] = file->before.st_atim;
        times[1] = file->before.st_mtim;
#endif
        if (futimens(fd, times)) {
            int first_errno = errno;
            struct timeval fallback[2];
            for (int i = 0; i < 2; ++i) {
                fallback[i].tv_sec = times[i].tv_sec;
                fallback[i].tv_usec = (suseconds_t)(times[i].tv_nsec / 1000);
            }
            if (futimes(fd, fallback)) {
                int fallback_errno = errno;
                if (first_errno != ENOSYS || fallback_errno != ENOSYS)
                    return fail(err, "Gecici dosyanin tarihleri korunamadi: %s (%s; futimens=%d, futimes=%d)",
                                file->path, strerror(fallback_errno), first_errno, fallback_errno);
                /* Some PS5 firmware variants lack both timestamp syscalls.
                 * Keep the default filesystem timestamp there; AMPR record
                 * timestamps are bytes in the index and remain unchanged. */
                static int warned;
                if (!warned) {
                    note("FC27 TR: bu firmware dosya tarihi ayarini desteklemiyor; indeks tarihleri korunuyor.");
                    warned = 1;
                }
            }
        }
    }
    if (fsync(fd))
        return fail(err, "Dosya diske yazilamadi: %s (%s)", file->path, strerror(errno));
    return 0;
}

static int write_all(int fd, const void *bytes, size_t length, char *err) {
    const unsigned char *data = bytes;
    while (length) {
        ssize_t n = write(fd, data, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return fail(err, "Yazma hatasi: %s", strerror(errno));
        data += (size_t)n;
        length -= (size_t)n;
    }
    return 0;
}

typedef struct {
    uint64_t previous, total;
    unsigned next;
    tr_bundle *bundle;
} progress_state;
static void extraction_progress(uint64_t done, uint64_t total, void *opaque) {
    progress_state *state = opaque;
    (void)total;
    if (cancelled) {
        bundle_request_cancel(state->bundle);
        return;
    }
    uint64_t global = state->previous + done;
    unsigned percent = state->total ? (unsigned)(global * 100 / state->total) : 100;
    while (state->next <= 75 && percent >= state->next) {
        note("FC27 TR: dosyalar dogrulaniyor ve hazirlaniyor: %u%%", state->next);
        state->next += 25;
    }
}

static int check_space(const char *game, uint64_t needed, char *err) {
    struct statvfs fs;
    if (statvfs(game, &fs) || !fs.f_frsize)
        return fail(err, "Bos alan okunamadi; kurulum durduruldu.");
    uint64_t available;
    if (fs.f_bavail > UINT64_MAX / fs.f_frsize)
        available = UINT64_MAX;
    else
        available = (uint64_t)fs.f_bavail * fs.f_frsize;
    if (needed > UINT64_MAX - 2u * 1024u * 1024u ||
        available < needed + 2u * 1024u * 1024u)
        return fail(err, "Bos alan yetersiz. En az %llu MB gerekli.",
                    (unsigned long long)((needed + 2u * 1024u * 1024u + 1048575) / 1048576));
    return 0;
}

static int stage_index(transaction_file *file, const unsigned char *patched,
                       size_t length, char *err) {
    int fd = open(file->stage, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        return fail(err, "Indeks gecici dosyasi acilamadi (%s).", strerror(errno));
    file->stage_owned = 1;
    int result = write_all(fd, patched, length, err);
    if (!result)
        result = finish_stage(fd, file, err);
    if (close(fd) && !result)
        result = fail(err, "Indeks gecici dosyasi kapatilamadi.");
    return result;
}

static int same_file_bytes(const char *path, const unsigned char *expected,
                           size_t length, char *err) {
    unsigned char *bytes;
    size_t read_length;
    if (read_file(path, INDEX_MAX, &bytes, &read_length, NULL, err))
        return -1;
    int result = read_length == length && !memcmp(bytes, expected, length);
    free(bytes);
    if (!result)
        return fail(err, "Indeks geri okuma dogrulamasi basarisiz.");
    return 0;
}

static int transaction_rename(const char *from, const char *to) {
#ifdef TR_HOST_TEST
    test_rename_count++;
    if (test_fail_rename && test_rename_count == test_fail_rename) {
        fprintf(stderr, "TR_HOST_TEST injected commit rename failure %u\n", test_rename_count);
        errno = EIO;
        return -1;
    }
#endif
    return rename(from, to);
}

static int commit_file(transaction_file *file, char *err) {
    if (!file->change)
        return 0;
    if (original_unchanged(file, err) || must_be_missing(file->previous, err))
        return -1;
    if (file->exists) {
        if (transaction_rename(file->path, file->previous))
            return fail(err, "Orijinal dosya ayirilamadi: %s (%s)", file->path, strerror(errno));
        file->backup_owned = 1;
    }
    if (transaction_rename(file->stage, file->path))
        return fail(err, "Dosya yerlestirilemedi: %s (%s)", file->path, strerror(errno));
    file->stage_owned = 0;
    file->committed = 1;
    return 0;
}

static int rollback(transaction_file files[TR_ASSET_COUNT + 1]) {
    int good = 1;
    for (int i = TR_ASSET_COUNT; i >= 0; --i) {
        transaction_file *file = &files[i];
        if (file->backup_owned) {
            if (rename(file->previous, file->path))
                good = 0;
            else {
                file->backup_owned = 0;
                file->committed = 0;
            }
        } else if (file->committed && !file->exists) {
            if (unlink(file->path) && errno != ENOENT)
                good = 0;
            else
                file->committed = 0;
        }
        if (file->stage_owned) {
            if (unlink(file->stage) && errno != ENOENT)
                good = 0;
            else
                file->stage_owned = 0;
        }
    }
    return good ? 0 : -1;
}

static int remove_created_dirs(void) {
    int good = 1;
    for (size_t i = created_dir_count; i > 0; --i)
        if (rmdir(created_dirs[i - 1]) && errno != ENOENT)
            good = 0;
    created_dir_count = 0;
    return good ? 0 : -1;
}

typedef struct {
    struct stat directory;
    struct stat index_stat;
    struct stat param_stat;
    char index_path[TR_PATH];
    char param_path[TR_PATH];
    unsigned char *index;
    unsigned char *param;
    size_t index_length;
    size_t param_length;
} overlay_source_snapshot;

static int snapshot_overlay_source(const options *opt, overlay_source_snapshot *source,
                                    char *err) {
    if (lstat(opt->game, &source->directory) || !S_ISDIR(source->directory.st_mode) ||
        path_join(source->index_path, opt->game, "ampr_emu.index", err) ||
        path_join(source->param_path, opt->game, "sce_sys/param.json", err))
        return fail(err, "Overlay kaynak klasoru dogrulanamadi.");
    if (read_file(source->index_path, INDEX_MAX, &source->index, &source->index_length,
                  &source->index_stat, err) ||
        read_file(source->param_path, PARAM_MAX, &source->param, &source->param_length,
                  &source->param_stat, err))
        return -1;
    return 0;
}

static int overlay_source_unchanged(const options *opt,
                                    const overlay_source_snapshot *source, char *err) {
    struct stat now;
    if (verify_overlay_source(opt, err) || reject_asset_pack(opt->game, err) ||
        reject_asset_pack(opt->overlay, err))
        return -1;
    if (lstat(opt->game, &now) || !S_ISDIR(now.st_mode) ||
        now.st_dev != source->directory.st_dev || now.st_ino != source->directory.st_ino ||
        lstat(source->index_path, &now) || !stat_same(&now, &source->index_stat) ||
        lstat(source->param_path, &now) || !stat_same(&now, &source->param_stat))
        return fail(err, "Overlay kaynak oyunu kurulum sirasinda degisti; islem durduruldu.");
    if (same_file_bytes(source->index_path, source->index, source->index_length, err) ||
        same_file_bytes(source->param_path, source->param, source->param_length, err))
        return -1;
    return 0;
}

static int install(options *opt, char *err) {
    int result = -1, version003 = 0, lock_owned = 0, lock_fd = -1;
    int overlay_mode = opt->overlay[0] != 0;
    char lock_path[TR_PATH] = {0}, anchor[TR_PATH] = {0};
    struct stat anchor_stat;
    const char *target = NULL;
    tr_bundle *bundle = NULL;
    unsigned char *index_original = NULL, *index_patched = NULL, *source_patched = NULL;
    size_t index_length = 0;
    overlay_source_snapshot source;
    memset(&source, 0, sizeof(source));
    transaction_file files[TR_ASSET_COUNT + 1];
    memset(files, 0, sizeof(files));
    created_dir_count = 0;
    if ((!opt->zip[0] && discover_zip(opt->zip, err)) ||
        (!opt->game[0] && discover_game(opt->game, err)))
        goto done;
    if (overlay_mode) {
        if (verify_overlay_source(opt, err) || canonical_game_path(opt->game, 1, err) ||
            prepare_overlay_path(opt, anchor, &anchor_stat, err))
            goto done;
        target = opt->overlay;
        if (reject_asset_pack(opt->game, err) || reject_asset_pack(target, err))
            goto done;
    } else {
        if (canonical_game(opt->game, err))
            goto done;
        target = opt->game;
        strcpy(anchor, target);
    }
    if (validate_param(opt->game, &version003, err))
        goto done;
    note("FC27 TR: %s basladi. Surum %s.", opt->check_only ? "kontrol" : "kurulum",
         version003 ? "01.000.003 (oyun uyumlulugu henuz dogrulanmadi)" : "01.000.004");
    printf("Kaynak: %s\nHedef: %s\nZIP: %s\n", opt->game, target, opt->zip);
    if (overlay_mode)
        note("FC27 TR: kaynak goruntu degistirilmeden secili overlay klasorune kurulacak.");
    if (bundle_open(opt->zip, &bundle, err, TR_ERR))
        goto done;
    unsigned changes;
    transaction_file *index_file = &files[TR_ASSET_COUNT];
    if (prepare_transaction_file(index_file, target, "ampr_emu.index", err))
        goto done;
    if (!overlay_mode && !index_file->exists) {
        fail(err, "ampr_emu.index bulunamadi.");
        goto done;
    }
    if (overlay_mode) {
        unsigned source_changes;
        if (snapshot_overlay_source(opt, &source, err) ||
            patch_index(source.index, source.index_length, &source_patched, &source_changes, err))
            goto done;
    }
    const char *index_origin = index_file->exists ? index_file->path : source.index_path;
    if (read_file(index_origin, INDEX_MAX, &index_original, &index_length,
                  index_file->exists ? &index_file->before : NULL, err) ||
        patch_index(index_original, index_length, &index_patched, &changes, err))
        goto done;
    if (overlay_mode && (index_length != source.index_length ||
                         memcmp(index_patched, source_patched, index_length))) {
        fail(err, "Mevcut overlay indeksi kaynak oyunla uyusmuyor; diger kayitlar degistirilmedi.");
        goto done;
    }
    free(source_patched);
    source_patched = NULL;
    index_file->change = changes != 0 || !index_file->exists;
    uint64_t total_needed = index_file->change ? index_length : 0;
    uint64_t audio_needed = 0;
    unsigned changed_files = 0;
    for (int i = 0; i < TR_ASSET_COUNT; ++i) {
        if (prepare_transaction_file(&files[i], target, tr_assets[i].path, err))
            goto done;
        char hash_error[TR_ERR];
        files[i].change = !files[i].exists ||
            bundle_hash_file(files[i].path, tr_assets[i].size, tr_assets[i].sha256hex,
                             hash_error, sizeof(hash_error)) != 0;
        if (files[i].change) {
            if (UINT64_MAX - total_needed < tr_assets[i].size) {
                fail(err, "Kurulum boyutu sinir disi.");
                goto done;
            }
            total_needed += tr_assets[i].size;
            audio_needed += tr_assets[i].size;
            changed_files++;
        }
    }
    if (path_join(lock_path, target, ".FC27_TR_INSTALL.lock", err) ||
        must_be_missing(lock_path, err))
        goto done;
    int game_closed = game_is_closed(err);
    if (game_closed < 0)
        goto done;
    if (opt->check_only) {
        if (changed_files || index_file->change)
            note("FC27 TR kontrol: %u dosya, %u indeks boyutu; indeks yazimi %s. Yazma yapilmadi.",
                 changed_files, changes, index_file->change ? "gerekli" : "gerekmiyor");
        else
            note("FC27 TR kontrol: zaten kurulu, 10 dosyanin SHA256 ve indeks boyutlari dogru.");
        if (!game_closed)
            note("FC27 su anda acik. Kurulum icin tamamen kapatin.");
        result = 0;
        goto done;
    }
    if (!game_closed) {
        fail(err, "FC27 acik. Oyunu tamamen kapatip yeniden deneyin.");
        goto done;
    }
    if (!changed_files && !index_file->change) {
        note("FC27 TR zaten kurulu: 10 dosyanin SHA256 ve indeks boyutlari dogru. Degisiklik yapilmadi.");
        result = 0;
        goto done;
    }
    if (check_space(anchor, total_needed, err) || require_closed(err))
        goto done;
    if (overlay_mode) {
        struct stat now;
        if (overlay_source_unchanged(opt, &source, err))
            goto done;
        if (lstat(anchor, &now) || !S_ISDIR(now.st_mode) ||
            now.st_dev != anchor_stat.st_dev || now.st_ino != anchor_stat.st_ino) {
            fail(err, "Overlay ust klasoru kurulum sirasinda degisti.");
            goto done;
        }
        if (parent_dirs(anchor, lock_path, 1, err))
            goto done;
    }
    lock_fd = open(lock_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (lock_fd < 0) {
        fail(err, "Kurulum kilidi olusturulamadi: %s", strerror(errno));
        goto done;
    }
    lock_owned = 1;
    char pid_text[32];
    int n = snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)getpid());
    if (write_all(lock_fd, pid_text, (size_t)n, err) || fsync(lock_fd)) {
        if (!err[0])
            fail(err, "Kurulum kilidi diske yazilamadi.");
        goto done;
    }
    if (close(lock_fd)) {
        lock_fd = -1;
        fail(err, "Kurulum kilidi kapatilamadi.");
        goto done;
    }
    lock_fd = -1;
    progress_state progress = {0, audio_needed, 25, bundle};
    for (int i = 0; i < TR_ASSET_COUNT; ++i) {
        if (!files[i].change)
            continue;
        if (cancelled) {
            fail(err, "Kurulum iptal edildi.");
            goto done;
        }
        if (parent_dirs(target, files[i].path, 1, err))
            goto done;
        int fd = open(files[i].stage, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (fd < 0) {
            fail(err, "Gecici ses dosyasi acilamadi: %s (%s)", files[i].path, strerror(errno));
            goto done;
        }
        files[i].stage_owned = 1;
        int extracted = bundle_extract_asset(bundle, (size_t)i, fd, extraction_progress,
                                             &progress, err, TR_ERR);
        if (!extracted)
            extracted = finish_stage(fd, &files[i], err);
        if (close(fd) && !extracted)
            extracted = fail(err, "Gecici ses dosyasi kapatilamadi.");
        if (cancelled)
            extracted = fail(err, "Kurulum iptal edildi.");
        if (extracted)
            goto done;
        progress.previous += tr_assets[i].size;
    }
    if (index_file->change && (stage_index(index_file, index_patched, index_length, err) ||
                              same_file_bytes(index_file->stage, index_patched, index_length, err)))
        goto done;
    if (require_closed(err) || same_file_bytes(index_origin, index_original, index_length, err) ||
        (overlay_mode && overlay_source_unchanged(opt, &source, err)))
        goto done;
    for (int i = 0; i < TR_ASSET_COUNT + 1; ++i)
        if (original_unchanged(&files[i], err))
            goto done;
    note("FC27 TR: ZIP CRC ve SHA256 dogru. Dosyalar yerlestiriliyor.");
    for (int i = 0; i < TR_ASSET_COUNT; ++i)
        if (files[i].change && (require_closed(err) || commit_file(&files[i], err)))
            goto done;
    if (require_closed(err) || commit_file(index_file, err) ||
        same_file_bytes(index_file->path, index_patched, index_length, err))
        goto done;
    note("FC27 TR: yerlestirilen dosyalarin SHA256 geri okumasi yapiliyor.");
#ifdef TR_OVERLAY_TEST
    if (test_overlay_final_hash_hook)
        test_overlay_final_hash_hook(opt);
#endif
    for (int i = 0; i < TR_ASSET_COUNT; ++i)
        if (bundle_hash_file(files[i].path, tr_assets[i].size,
                             tr_assets[i].sha256hex, err, TR_ERR))
            goto done;
    if (overlay_mode && overlay_source_unchanged(opt, &source, err))
        goto done;
#ifndef TR_HOST_TEST
    if (overlay_mode) {
        note("FC27 TR: oyunun gorecegi Turkce dosyalar dogrulaniyor.");
        char visible[TR_PATH];
        for (int i = 0; i < TR_ASSET_COUNT; ++i)
            if (path_join(visible, SM_OVERLAY_RUNTIME, tr_assets[i].path, err) ||
                bundle_hash_file(visible, tr_assets[i].size, tr_assets[i].sha256hex, err, TR_ERR))
                goto done;
        if (path_join(visible, SM_OVERLAY_RUNTIME, "ampr_emu.index", err) ||
            same_file_bytes(visible, index_patched, index_length, err) ||
            overlay_source_unchanged(opt, &source, err))
            goto done;
    }
#endif
    result = 0;
    int cleanup_good = 1;
    for (int i = 0; i < TR_ASSET_COUNT + 1; ++i)
        if (files[i].backup_owned) {
            if (unlink(files[i].previous))
                cleanup_good = 0;
            else
                files[i].backup_owned = 0;
        }
    if (overlay_mode)
        note("FC27 TR: %u dosya ve indeks dogrulandi. Oyun baglamasi hazirlaniyor.", changed_files);
    else
        note("FC27 TR kurulum tamamlandi: %u dosya, %u indeks kaydi. Oyunda Turkce spikeri secin.",
             changed_files, changes);
    if (!cleanup_good)
        note("Kurulum dogru; bazi .tr-previous gecici kopyalar temizlenemedi.");

done:
    if (result && rollback(files))
        note("Geri alma tamamlanamadi. .tr-previous dosyalarini koruyun; installer.log kaydini inceleyin.");
    if (lock_fd >= 0)
        close(lock_fd);
    if (lock_owned && unlink(lock_path))
        note("Kurulum kilidi temizlenemedi: %s", lock_path);
    if (result && remove_created_dirs())
        note("Bazi yeni bos klasorler temizlenemedi; oyun dosyalarinin geri alma sonucunu kontrol edin.");
    free(source.index);
    free(source.param);
    free(source_patched);
    free(index_original);
    free(index_patched);
    bundle_close(bundle);
    return result;
}

int main(int argc, char **argv) {
    options opt;
    memset(&opt, 0, sizeof(opt));
    char err[TR_ERR] = {0};
    signal(SIGINT, signal_cancel);
    signal(SIGTERM, signal_cancel);
    start_log();
    note("FC27 TR v0.2.0-beta (KLASOR+GORUNTU) baslatiliyor.");
#ifdef TR_HOST_TEST
    const char *fail_env = getenv("TR_FAIL_COMMIT");
    if (fail_env) {
        char *end;
        unsigned long value = strtoul(fail_env, &end, 10);
        if (!*fail_env || *end || !value || value > 64) {
            fail(err, "TR_FAIL_COMMIT 1..64 olmali.");
            goto error;
        }
        test_fail_rename = (unsigned)value;
    }
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--check"))
            opt.check_only = 1;
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) {
            if (copy_path(opt.game, argv[++i], err))
                goto error;
        } else if (!strcmp(argv[i], "--overlay") && i + 1 < argc) {
            if (copy_path(opt.overlay, argv[++i], err))
                goto error;
        } else if (!strcmp(argv[i], "--zip") && i + 1 < argc) {
            if (copy_path(opt.zip, argv[++i], err))
                goto error;
        } else if (!strcmp(argv[i], "--fail-commit") && i + 1 < argc) {
            char *end;
            const char *text = argv[++i];
            unsigned long value = strtoul(text, &end, 10);
            if (!*text || *end || !value || value > 64) {
                fail(err, "--fail-commit 1..64 olmali.");
                goto error;
            }
            test_fail_rename = (unsigned)value;
        } else {
            fail(err, "Kullanim: installer --check --game /oyun --zip /paket.zip [--overlay /hedef]");
            goto error;
        }
    }
#else
    (void)argc;
    (void)argv;
    if (load_config(&opt, err))
        goto error;
    if ((!opt.zip[0] && discover_zip(opt.zip, err)) ||
        prepare_native_target(&opt, err))
        goto error;
#endif
    if (install(&opt, err))
        goto error;
#ifndef TR_HOST_TEST
    if (image_context_initialized && !opt.check_only) {
        if (image_context.hooks.wait_stable(image_context.hooks.opaque, image_context.target, err) ||
            sm_overlay_revalidate(&image_context, err))
            goto error;
    }
    if (cleanup_native_target()) {
        fail(err, "Dosya islemi bitti ancak baglama temizligi tamamlanamadi; onceki bildirimi kontrol edin.");
        goto error;
    }
    if (opt.overlay[0] && !opt.check_only)
        note("FC27 TR goruntu kurulumu tamamlandi. Oyunda Turkce spikeri secin.");
#endif
    if (log_file)
        fclose(log_file);
    return 0;
error:
#ifndef TR_HOST_TEST
    cleanup_native_target();
#endif
    note("FC27 TR HATA: %s", err[0] ? err : "Bilinmeyen hata. Orijinaller korunuyor.");
    if (log_file)
        fclose(log_file);
    return 1;
}
