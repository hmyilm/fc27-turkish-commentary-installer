/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shadowmount.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMAGE "/data/etaHEN/games/PPSA34015.ffpfsc"
#define SOURCE "/mnt/shadowmnt/PPSA34015_01768c84"
#define CUSTOM "/mnt/usb0/Games/backports/PPSA34015"
static unsigned tests;
#define REQUIRE(x) do { ++tests; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

typedef struct {
    int mounted, ro, closed, fallback, homebrew, backports, custom;
    int mount_calls, unmount_calls, mkdir_calls, rmdir_calls, info_calls;
    int deny_mount, deny_unmount, bad_mount_reply, fail_info, wrong_selection;
    int no_selection, target_nonempty, changed_device, changed_source, changed_target;
    int changed_image, changed_fsid, forbidden_source_fs, wrong_title_source;
    int escaped_json, late_other_mount, stable_calls, deny_stability, writable_upper;
    char selected[SM_OVERLAY_PATH];
    const char *override_json;
} fixture;

static int fclosed(void *v, char *err) {
    fixture *f = v;
    if (f->closed) return 0;
    strcpy(err, "game running"); return -1;
}
static int fstable(void *v, const char *p, char *err) {
    fixture *f = v;
    REQUIRE(!strcmp(p, SM_OVERLAY_FALLBACK));
    REQUIRE(f->fallback && !f->mounted);
    ++f->stable_calls;
    if (f->deny_stability) { strcpy(err, "stability wait interrupted"); return -1; }
    return 0;
}
static void identity(sm_overlay_file *i, int kind, uint64_t dev, uint64_t ino) {
    memset(i, 0, sizeof(*i)); i->kind = kind; i->device = dev; i->inode = ino;
    i->size = kind == 1 ? 19000000000ull : 0;
    i->mtime_sec = 100; i->ctime_sec = 100;
}
static int fprobe(void *v, const char *p, sm_overlay_file *i, char *err) {
    (void)err;
    fixture *f = v;
    memset(i, 0, sizeof(*i));
    if (!strcmp(p, IMAGE)) { identity(i, 1, 1, 50); i->size += f->changed_image; }
    else if (!strcmp(p, SOURCE) && f->mounted) identity(i, 2, 9, 60 + f->changed_source);
    else if (!strcmp(p, "/data")) identity(i, 2, 1, 1);
    else if (!strcmp(p, "/data/homebrew") && f->homebrew) identity(i, 2, 1 + f->changed_device, 2);
    else if (!strcmp(p, "/data/homebrew/backports") && f->backports) identity(i, 2, 1, 3);
    else if (!strcmp(p, SM_OVERLAY_FALLBACK) && f->fallback) identity(i, 2, 1, 4 + f->changed_target);
    else if (!strcmp(p, CUSTOM) && f->custom) identity(i, 2, 5, 5 + f->changed_target);
    return 0;
}
static int fmount(void *v, const char *p, sm_overlay_mount *m, char *err) {
    fixture *f = v;
    memset(m, 0, sizeof(*m));
    if (!f->mounted) { strcpy(err, "not mounted"); return -1; }
    m->read_only = f->ro;
    m->fsid[0] = 10 + f->changed_fsid;
    if (!strcmp(p, SOURCE)) {
        strcpy(m->point, SOURCE); strcpy(m->from, "/dev/lvd13");
        strcpy(m->type, f->forbidden_source_fs ? "unionfs" : "exfatfs");
    } else if (!strcmp(p, SM_OVERLAY_RUNTIME)) {
        strcpy(m->point, SM_OVERLAY_RUNTIME);
        if (f->selected[0]) {
            strcpy(m->type, "unionfs");
            if (f->writable_upper) m->read_only = 0;
            snprintf(m->from, sizeof(m->from), "<above>:%s", f->selected);
        } else {
            strcpy(m->type, "nullfs");
            strcpy(m->from, f->wrong_title_source ? "/mnt/shadowmnt/OTHER" : SOURCE);
        }
        m->fsid[1] = 11;
    } else { strcpy(err, "unexpected mount path"); return -1; }
    return 0;
}
static int fmkdir(void *v, const char *p, char *err) {
    fixture *f = v;
    ++f->mkdir_calls;
    if (!strcmp(p, "/data/homebrew")) f->homebrew = 1;
    else if (!strcmp(p, "/data/homebrew/backports")) f->backports = 1;
    else if (!strcmp(p, SM_OVERLAY_FALLBACK)) f->fallback = 1;
    else { strcpy(err, "unexpected mkdir"); return -1; }
    return 0;
}
static int frmdir(void *v, const char *p, char *err) {
    fixture *f = v;
    ++f->rmdir_calls;
    if (f->target_nonempty) return 1;
    if (!strcmp(p, SM_OVERLAY_FALLBACK)) f->fallback = 0;
    else if (!strcmp(p, "/data/homebrew/backports")) f->backports = 0;
    else if (!strcmp(p, "/data/homebrew")) f->homebrew = 0;
    else { strcpy(err, "unexpected rmdir"); return -1; }
    return 0;
}
static int frequest(void *v, const char *route, const char *body, char *out,
                    size_t cap, int *status, char *err) {
    fixture *f = v;
    *status = 200;
    REQUIRE(strstr(body, SM_OVERLAY_TITLE) != NULL);
    if (!strcmp(route, "/api/v1/games/info")) {
        ++f->info_calls;
        if (f->fail_info) { strcpy(err, "transport timeout"); return -1; }
        if (f->override_json) { snprintf(out, cap, "%s", f->override_json); return 0; }
        snprintf(out, cap,
            "{\"status\":0,\"title_id\":\"%s\",\"path\":\"%s\","
            "\"runtime_path\":\"%s\",\"source_type\":\"image\","
            "\"image_backed\":true,\"managed\":true,\"mounted\":%s,"
            "\"source_available\":true,\"extra\":{\"nested\":[null,1.2e+3,true]}}",
            f->escaped_json ? "PPSA3401\\u0035" : SM_OVERLAY_TITLE, IMAGE,
            SOURCE, f->mounted ? "true" : "false");
        if (f->late_other_mount && f->info_calls == 1) f->mounted = 1;
    } else if (!strcmp(route, "/api/v1/games/mount")) {
        ++f->mount_calls;
        REQUIRE(strstr(body, "\"mode\":\"ro\"") != NULL);
        if (f->deny_mount) { *status = 409; strcpy(out, "{\"status\":16}"); return 0; }
        f->mounted = 1; f->ro = 1;
        if (f->fallback && !f->no_selection)
            strcpy(f->selected, f->wrong_selection ? CUSTOM : SM_OVERLAY_FALLBACK);
        snprintf(out, cap, "{\"status\":0,\"title_id\":\"%s\",\"mounted\":true,\"mode\":\"%s\"}",
                 SM_OVERLAY_TITLE, f->bad_mount_reply ? "rw" : "ro");
    } else if (!strcmp(route, "/api/v1/games/unmount")) {
        ++f->unmount_calls;
        if (f->deny_unmount) { *status = 409; strcpy(out, "{\"status\":16}"); return 0; }
        f->mounted = 0; f->selected[0] = 0;
        snprintf(out, cap, "{\"status\":0,\"title_id\":\"%s\",\"mounted\":false}", SM_OVERLAY_TITLE);
    } else { strcpy(err, "unexpected route"); return -1; }
    return 0;
}
static void setup(fixture *f, sm_overlay_context *c) {
    memset(f, 0, sizeof(*f)); f->closed = 1; f->ro = 1;
    sm_overlay_hooks h = {f, frequest, fprobe, fmount, fmkdir, frmdir, fclosed, fstable};
    sm_overlay_init(c, &h);
}
static void existing(fixture *f) {
    f->mounted = 1; f->custom = 1; strcpy(f->selected, CUSTOM);
}

static void lifecycles(void) {
    fixture f; sm_overlay_context c; char e[SM_OVERLAY_ERR] = {0};
    setup(&f, &c);
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0);
    REQUIRE(c.prepared && c.overlay_proven && !c.target_missing && c.owns_mount);
    REQUIRE(!strcmp(c.source, SOURCE) && !strcmp(c.image, IMAGE));
    REQUIRE(!strcmp(c.target, SM_OVERLAY_FALLBACK));
    REQUIRE(f.mount_calls == 2 && f.unmount_calls == 1 && f.mkdir_calls == 3);
    REQUIRE(f.stable_calls == 1);
    REQUIRE(sm_overlay_revalidate(&c, e) == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0);
    REQUIRE(f.unmount_calls == 2 && f.rmdir_calls == 3 && !f.fallback && !f.mounted);

    setup(&f, &c);
    REQUIRE(sm_overlay_prepare(&c, IMAGE, 0, e) == 0);
    f.target_nonempty = 1;
    REQUIRE(sm_overlay_cleanup(&c, e) == 0);
    REQUIRE(f.fallback && f.rmdir_calls == 1 && !f.mounted);

    setup(&f, &c);
    REQUIRE(sm_overlay_prepare(&c, NULL, 1, e) == 0);
    REQUIRE(c.target_missing && !c.overlay_proven && c.check_only);
    REQUIRE(f.mkdir_calls == 0 && f.mount_calls == 1);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.rmdir_calls == 0 && !f.mounted);

    setup(&f, &c); existing(&f);
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0);
    REQUIRE(!strcmp(c.target, CUSTOM) && !c.owns_mount && c.overlay_proven);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.mounted);
    REQUIRE(f.mount_calls == 0 && f.unmount_calls == 0 && f.mkdir_calls == 0);

    /* Actual PS5 unionfs flags reflect upper storage while the image stays RO. */
    setup(&f, &c); existing(&f); f.writable_upper = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0 && !c.title_mount.read_only);
    REQUIRE(c.source_mount.read_only && sm_overlay_revalidate(&c, e) == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.mounted);

    setup(&f, &c); f.mounted = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0);
    REQUIRE(f.mkdir_calls == 0 && f.unmount_calls == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.mounted);

    setup(&f, &c); f.mounted = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 1, e) == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.mounted && !f.unmount_calls);

    /* A mount appearing after the first info response belongs to somebody
     * else. No acquisition or cleanup of it is allowed. */
    setup(&f, &c); f.late_other_mount = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 1, e) == 0);
    REQUIRE(!c.owns_mount && !f.mount_calls && !f.mkdir_calls);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.mounted && !f.unmount_calls);

    setup(&f, &c); f.homebrew = f.backports = f.fallback = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0);
    REQUIRE(f.mount_calls == 1 && f.mkdir_calls == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.fallback && f.rmdir_calls == 0);

    setup(&f, &c); f.escaped_json = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 1, e) == 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0);
}

static void failures(void) {
    fixture f; sm_overlay_context c; char e[SM_OVERLAY_ERR] = {0};
    setup(&f, &c); f.deny_stability = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && f.mount_calls == 1);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && !f.fallback && !f.mounted);
    setup(&f, &c); f.closed = 0;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.info_calls && !f.mount_calls);
    setup(&f, &c); c.hooks.game_closed = NULL;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.info_calls && !f.mount_calls);
    setup(&f, &c); f.deny_mount = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !c.owns_mount);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && !f.unmount_calls);
    setup(&f, &c); f.bad_mount_reply = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !c.owns_mount);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && !f.unmount_calls);
    setup(&f, &c); f.fail_info = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mount_calls);
    setup(&f, &c);
    REQUIRE(sm_overlay_prepare(&c, "/data/Other.ffpfsc", 0, e) != 0 && !f.mount_calls);
    setup(&f, &c); existing(&f); f.ro = 0;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mount_calls && !f.unmount_calls);
    setup(&f, &c); f.forbidden_source_fs = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && c.owns_mount);
    REQUIRE(sm_overlay_cleanup(&c, e) != 0 && !f.unmount_calls);
    setup(&f, &c); f.wrong_title_source = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0);
    REQUIRE(sm_overlay_cleanup(&c, e) != 0 && !f.unmount_calls);
    setup(&f, &c); f.homebrew = 1; f.changed_device = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && f.mkdir_calls == 0);
    setup(&f, &c); f.no_selection = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && f.mkdir_calls == 3);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && !f.fallback);
    setup(&f, &c); f.wrong_selection = 1; f.custom = 1;
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0);
    REQUIRE(sm_overlay_cleanup(&c, e) == 0 && f.custom);
    setup(&f, &c); existing(&f); strcpy(f.selected, "/data/backports/PPSA00000");
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mkdir_calls);
    setup(&f, &c); existing(&f); strcpy(f.selected, "/user/app/backports/PPSA34015");
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mkdir_calls);
    setup(&f, &c); existing(&f); strcpy(f.selected, "/data/x/../backports/PPSA34015");
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mkdir_calls);

    for (int mutation = 0; mutation < 7; ++mutation) {
        setup(&f, &c);
        REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0);
        if (mutation == 0) f.changed_image = 1;
        if (mutation == 1) f.changed_source = 1;
        if (mutation == 2) f.changed_target = 1;
        if (mutation == 3) f.changed_fsid = 1;
        if (mutation == 4) f.closed = 0;
        if (mutation == 5) f.mounted = 0;
        if (mutation == 6) f.ro = 0;
        REQUIRE(sm_overlay_revalidate(&c, e) != 0);
        if (mutation == 0 || mutation == 1 || mutation == 3 || mutation == 4 || mutation == 6) {
            int before = f.unmount_calls;
            REQUIRE(sm_overlay_cleanup(&c, e) != 0 && f.unmount_calls == before);
        }
    }
    setup(&f, &c);
    REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) == 0);
    f.deny_unmount = 1;
    REQUIRE(sm_overlay_cleanup(&c, e) != 0 && f.rmdir_calls == 0 && f.mounted);
}

static void json_failures(void) {
    const char *bad[] = {
        "{}", "[]", "{\"status\":0,\"status\":0}", "{\"status\":0,\"statu\\u0073\":0}",
        "{\"status\":-1}", "{\"status\":0.1}", "{\"status\":01}", "{\"status\":2147483648}",
        "{\"status\":0,}", "{\"status\":0}garbage", "{\"status\":0,\"mounted\":1}",
        "{\"status\":0,\"title_id\":\"PPSA34015\\u0000\"}",
        "{\"status\":0,\"path\":\"\\ud800\"}", "{\"status\":0,\"path\":\"\\udc00\"}",
        "{\"status\":0,\"extra\":[true false]}", "{\"status\":0,\"extra\":tru}",
        "{\"status\":0,\"extra\":1e}", "{\"status\":0,\"extra\":1.}",
        "{\"status\":0,\"extra\":{\"a\":true,}}"
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        fixture f; sm_overlay_context c; char e[SM_OVERLAY_ERR];
        setup(&f, &c); f.override_json = bad[i];
        REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mount_calls && !f.mkdir_calls);
    }
    const char *fields[] = {
        "\"title_id\":\"PPSA00000\"", "\"path\":\"/data/a/../PPSA34015.ffpfsc\"",
        "\"runtime_path\":\"/system_ex/app/PPSA34015\"", "\"source_type\":\"folder\"",
        "\"image_backed\":false", "\"managed\":false", "\"source_available\":false"
    };
    const char *original[] = {
        "\"title_id\":\"PPSA34015\"", "\"path\":\"" IMAGE "\"",
        "\"runtime_path\":\"" SOURCE "\"", "\"source_type\":\"image\"",
        "\"image_backed\":true", "\"managed\":true", "\"source_available\":true"
    };
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i) {
        fixture f; sm_overlay_context c; char e[SM_OVERLAY_ERR], json[8192], modified[8192]; int status;
        setup(&f, &c);
        frequest(&f, "/api/v1/games/info", "PPSA34015", json, sizeof(json), &status, e);
        char *at = strstr(json, original[i]); REQUIRE(at != NULL);
        *at = 0;
        snprintf(modified, sizeof(modified), "%s%s%s", json, fields[i], at + strlen(original[i]));
        f.override_json = modified;
        REQUIRE(sm_overlay_prepare(&c, NULL, 0, e) != 0 && !f.mount_calls && !f.mkdir_calls);
    }
}

static void http_tests(void) {
    char out[128], e[SM_OVERLAY_ERR]; int status;
    const char *good = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n{\"status\":0}";
    REQUIRE(sm_overlay_test_http(good, strlen(good), out, sizeof(out), &status, e) == 0);
    REQUIRE(status == 200 && !strcmp(out, "{\"status\":0}"));
    const char *bad[] = {
        "HTTP/1.1 200 OK\r\n\r\n{}", "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 2\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nContent-Length: -2\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nContent-Length: 65537\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\nContent-Length: 2x\r\n\r\n{}",
        "HTTP/2 200 OK\r\nContent-Length: 2\r\n\r\n{}",
        "HTTP/1.1 xyz OK\r\nContent-Length: 2\r\n\r\n{}",
        "HTTP/1.1 200 OK\r\n Content-Length: 2\r\n\r\n{}"
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i)
        REQUIRE(sm_overlay_test_http(bad[i], strlen(bad[i]), out, sizeof(out), &status, e) != 0);
}

static void native_probe_tests(void) {
    char temporary[] = "/tmp/fc27-shadowmount-XXXXXX", e[SM_OVERLAY_ERR], file[1024], link[1024], child[1024];
    REQUIRE(mkdtemp(temporary) != NULL);
    /* macOS /tmp is a symlink: use canonical root for the positive case. */
    char *root = realpath(temporary, NULL); REQUIRE(root != NULL);
    snprintf(file, sizeof(file), "%s/file", root);
    snprintf(link, sizeof(link), "%s/link", root);
    snprintf(child, sizeof(child), "%s/link/missing", root);
    FILE *f = fopen(file, "w"); REQUIRE(f != NULL); fputs("test", f); fclose(f);
    REQUIRE(symlink(root, link) == 0);
    sm_overlay_context c; sm_overlay_init(&c, NULL); sm_overlay_file st;
    REQUIRE(c.hooks.probe(NULL, file, &st, e) == 0 && st.kind == 1 && st.size == 4);
    REQUIRE(c.hooks.probe(NULL, root, &st, e) == 0 && st.kind == 2);
    REQUIRE(c.hooks.probe(NULL, link, &st, e) != 0);
    REQUIRE(c.hooks.probe(NULL, child, &st, e) != 0);
    snprintf(child, sizeof(child), "%s/missing/deep", root);
    REQUIRE(c.hooks.probe(NULL, child, &st, e) == 0 && st.kind == 0);
    snprintf(child, sizeof(child), "%s/file/child", root);
    REQUIRE(c.hooks.probe(NULL, child, &st, e) != 0);
    unlink(link); unlink(file); rmdir(root); free(root);
}

int main(void) {
    lifecycles(); failures(); json_failures(); http_tests(); native_probe_tests();
    printf("ShadowMount module: %u assertions passed; mocked transport/filesystems, no console access.\n", tests);
    return 0;
}
