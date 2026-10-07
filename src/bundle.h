/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TR_BUNDLE_H
#define TR_BUNDLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct tr_bundle tr_bundle;
typedef void (*bundle_progress_fn)(uint64_t asset_bytes,
                                   uint64_t asset_total, void *opaque);

/* All functions return 0 on success and -1 on failure, except getters/close.
 * err may be NULL when err_cap is zero. The caller owns the opened output fd.
 * A bundle is single-threaded. Unknown ZIP entries are never extracted.
 */
int bundle_open(const char *zip_path, tr_bundle **out,
                char *err, size_t err_cap);
uint64_t bundle_total_bytes(const tr_bundle *bundle);

/* Output must be an empty, writable temporary fd positioned at byte zero.
 * Extracts one compiled-in asset, checks exact size, ZIP CRC32 and SHA256.
 * On failure the caller must discard the partial file. Success does not fsync
 * or close the output fd; the transactional installer performs those steps.
 * Progress is cumulative for this asset, not for the complete installation.
 */
int bundle_extract_asset(tr_bundle *bundle, size_t asset_index, int output_fd,
                         bundle_progress_fn progress, void *opaque,
                         char *err, size_t err_cap);
void bundle_close(tr_bundle *bundle);

/* Streams an existing regular file and verifies both its full size and hash.
 * The final path component must not be a symlink where O_NOFOLLOW is present.
 */
int bundle_hash_file(const char *path, uint64_t expected_size,
                     const char *expected_hash, char *err, size_t err_cap);

#endif
