/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TR_SHADOWMOUNT_H
#define TR_SHADOWMOUNT_H
#include <stddef.h>
#include <stdint.h>

#define SM_OVERLAY_PATH 1024
#define SM_OVERLAY_ERR 512
#define SM_OVERLAY_TITLE "PPSA34015"
#define SM_OVERLAY_RUNTIME "/system_ex/app/" SM_OVERLAY_TITLE
#define SM_OVERLAY_FALLBACK "/data/homebrew/backports/" SM_OVERLAY_TITLE

/* probe must reject symlinks (including ancestors). kind: 0 absent, 1 regular,
 * 2 directory. Directory timestamps are intentionally not identity fields. */
typedef struct {
    int kind;
    uint64_t device, inode, size;
    int64_t mtime_sec, mtime_nsec, ctime_sec, ctime_nsec;
} sm_overlay_file;

typedef struct {
    char point[SM_OVERLAY_PATH], from[SM_OVERLAY_PATH], type[32];
    uint32_t fsid[2];
    int read_only;
} sm_overlay_mount;

typedef struct {
    void *opaque;
    /* request returns a complete NUL-terminated JSON body and HTTP status.
     * Never follow redirects or send requests anywhere except loopback:10101. */
    int (*request)(void *, const char *route, const char *body, char *reply,
                   size_t reply_size, int *http_status, char *err);
    int (*probe)(void *, const char *path, sm_overlay_file *, char *err);
    int (*mount_info)(void *, const char *path, sm_overlay_mount *, char *err);
    int (*make_directory)(void *, const char *path, char *err);
    /* remove_directory: 0 removed, 1 nonempty (preserve), -1 failure. */
    int (*remove_directory)(void *, const char *path, char *err);
    /* Required. Return 0 only when the game is closed. Invoked before mount
     * changes and revalidation. Native caller supplies its existing check. */
    int (*game_closed)(void *, char *err);
    /* Wait for the loader's fresh-directory stability window before remount.
     * No files/settings are changed; native wait is bounded to 60 seconds. */
    int (*wait_stable)(void *, const char *path, char *err);
} sm_overlay_hooks;

typedef struct {
    char source[SM_OVERLAY_PATH]; /* Unoverlaid mounted image game directory. */
    char target[SM_OVERLAY_PATH]; /* Persistent, selected backport directory. */
    char image[SM_OVERLAY_PATH];
    int prepared, check_only, target_missing, overlay_proven, owns_mount;
    sm_overlay_hooks hooks;
    sm_overlay_file image_identity, source_identity, target_identity;
    sm_overlay_mount source_mount, title_mount;
    int have_mount_snapshot;
    char created[3][SM_OVERLAY_PATH];
    sm_overlay_file created_identity[3];
    size_t created_count;
} sm_overlay_context;

/* NULL platform callbacks use native implementations, except game_closed.
 * Initialize a fresh context once; cleanup also applies after prepare fails. */
void sm_overlay_init(sm_overlay_context *, const sm_overlay_hooks *);
/* selector optionally requires an exact physical image path. Only PPSA34015
 * managed images are accepted. This does NOT validate param/index contents:
 * installer must validate those at ctx.source using its existing checks.
 * check_only never creates files/directories; a missing fallback is reported
 * as target_missing=1 and overlay_proven=0, with no install permitted. */
int sm_overlay_prepare(sm_overlay_context *, const char *selector,
                       int check_only, char err[SM_OVERLAY_ERR]);
/* Call immediately before commit, after the core's file identity checks. */
int sm_overlay_revalidate(sm_overlay_context *, char err[SM_OVERLAY_ERR]);
/* Releases only an unchanged mount acquired by this context. Server-side
 * EBUSY is honored. Removes only unchanged, empty directories we created.
 * No force unmount and no recursive deletion. On uncertainty leaves state. */
int sm_overlay_cleanup(sm_overlay_context *, char err[SM_OVERLAY_ERR]);

#ifdef TR_SHADOWMOUNT_TEST
int sm_overlay_test_http(const char *, size_t, char *, size_t, int *, char *);
#endif
#endif
