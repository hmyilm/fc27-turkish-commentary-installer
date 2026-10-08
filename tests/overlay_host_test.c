/* SPDX-License-Identifier: GPL-3.0-or-later
 * Production transaction implementation with host-only deterministic hooks.
 */
#define TR_HOST_TEST
#define TR_OVERLAY_TEST
#define main installer_main_for_overlay_test
#include "../src/installer.c"
#undef main

static void mutate_source_before_commit(const options *opt, unsigned phase) {
    if (phase != 3)
        return;
    char path[TR_PATH], err[TR_ERR];
    if (path_join(path, opt->game, "ampr_emu.index", err))
        abort();
    if (chmod(path, 0600))
        abort();
    int fd = open(path, O_WRONLY);
    unsigned char bad = 0;
    if (fd < 0 || pwrite(fd, &bad, 1, 0) != 1 || close(fd))
        abort();
}

static void corrupt_unchanged_target(const options *opt) {
    char path[TR_PATH], err[TR_ERR];
    if (path_join(path, opt->overlay, tr_assets[0].path, err))
        abort();
    int fd = open(path, O_WRONLY);
    unsigned char bad = 0xff;
    if (fd < 0 || pwrite(fd, &bad, 1, 0) != 1 || close(fd))
        abort();
}

static void cancelling_progress(uint64_t done, uint64_t total, void *opaque) {
    if (done >= 32768)
        cancelled = 1;
    extraction_progress(done, total, opaque);
}

static int cancel_bundle(const char *zip, const char *output) {
    tr_bundle *bundle = NULL;
    char err[TR_ERR];
    if (bundle_open(zip, &bundle, err, sizeof(err)))
        return 1;
    int fd = open(output, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 2;
    progress_state state = {0, tr_assets[0].size, 25, bundle};
    int result = bundle_extract_asset(bundle, 0, fd, cancelling_progress,
                                      &state, err, sizeof(err));
    struct stat st;
    int passed = result != 0 && cancelled && !fstat(fd, &st) &&
                 st.st_size > 0 && st.st_size <= 131072 &&
                 (uint64_t)st.st_size < tr_assets[0].size;
    close(fd);
    bundle_close(bundle);
    if (!passed) {
        fprintf(stderr, "Cancellation failed: %s\n", err);
        return 3;
    }
    printf("cancelled after %lld bytes\n", (long long)st.st_size);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "cancel-bundle"))
        return cancel_bundle(argv[2], argv[3]);
    if (getenv("TR_TEST_SOURCE_CHANGE"))
        test_overlay_validation_hook = mutate_source_before_commit;
    if (getenv("TR_TEST_CORRUPT_UNCHANGED"))
        test_overlay_final_hash_hook = corrupt_unchanged_target;
    return installer_main_for_overlay_test(argc, argv);
}
