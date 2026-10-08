/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bundle.h"
#include "assets.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { uint64_t bytes, total; int bad; } progress_state;
static void progress(uint64_t bytes, uint64_t total, void *opaque)
{
    progress_state *state = opaque;
    if (bytes < state->bytes || bytes > total ||
        (state->total && state->total != total))
        state->bad = 1;
    state->bytes = bytes;
    state->total = total;
}

int main(int argc, char **argv)
{
    tr_bundle *bundle = NULL;
    char error[512], temporary[] = "/tmp/tr-bundle-hash-XXXXXX";
    uint64_t bytes = 0;
    size_t index, limit = TR_ASSET_COUNT;
    int fd;
    if (argc < 2 || argc > 3)
        return 2;
    if (bundle_open(argv[1], &bundle, error, sizeof(error))) {
        fprintf(stderr, "OPEN FAIL: %s\n", error);
        return 1;
    }
    if (argc == 3 && !strcmp(argv[2], "--open-only")) {
        printf("OPEN OK\n");
        bundle_close(bundle);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[2], "--first-only"))
        limit = 1;
    fd = open("/dev/null", O_WRONLY);
    if (fd < 0)
        return 2;
    for (index = 0; index < limit; ++index) {
        progress_state state = {0};
        if (bundle_extract_asset(bundle, index, fd, progress, &state,
                                  error, sizeof(error))) {
            fprintf(stderr, "EXTRACT FAIL: %s\n", error);
            close(fd);
            bundle_close(bundle);
            return 1;
        }
        if (state.bad || state.bytes != tr_assets[index].size)
            return 2;
        bytes += state.bytes;
        printf("CRC32 + SHA256 OK: %s (%" PRIu64 " bytes)\n",
               tr_assets[index].path, state.bytes);
        fflush(stdout);
    }
    close(fd);
    if (limit == TR_ASSET_COUNT && bytes != bundle_total_bytes(bundle))
        return 2;
    fd = mkstemp(temporary);
    if (fd < 0)
        return 2;
    if (bundle_extract_asset(bundle, 0, fd, NULL, NULL, error, sizeof(error)) ||
        bundle_hash_file(temporary, tr_assets[0].size,
                         tr_assets[0].sha256hex, error, sizeof(error))) {
        fprintf(stderr, "FILE HASH FAIL: %s\n", error);
        close(fd);
        unlink(temporary);
        return 1;
    }
    if (pwrite(fd, "X", 1, 0) != 1 ||
        bundle_hash_file(temporary, tr_assets[0].size,
                         tr_assets[0].sha256hex, error, sizeof(error)) != -1) {
        fprintf(stderr, "Did not reject changed file hash\n");
        return 1;
    }
    if (ftruncate(fd, (off_t)tr_assets[0].size - 1) ||
        bundle_hash_file(temporary, tr_assets[0].size,
                         tr_assets[0].sha256hex, error, sizeof(error)) != -1) {
        fprintf(stderr, "Did not reject changed file size\n");
        return 1;
    }
    close(fd);
    unlink(temporary);
    bundle_close(bundle);
    printf("Existing file hash/size rejection OK\nTOTAL VERIFIED: %" PRIu64 " bytes\n", bytes);
    return 0;
}
