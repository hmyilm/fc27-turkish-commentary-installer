/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bundle.h"
#include "assets.h"
#include "miniz.h"
#include "sha256.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
#error "The installer requires miniz ZIP CRC32 checks"
#endif

_Static_assert(sizeof(off_t) >= 8, "64-bit file offsets are required");

/* miniz central-directory and inflate allocations are bounded together.
 * Normal installation uses a small central directory and 32/64 KiB buffers.
 */
#define BUNDLE_HEAP_LIMIT (8U * 1024U * 1024U)
#define BUNDLE_MAX_ENTRIES 256U
#define BUNDLE_MAX_NAME 1024U
#define HASH_BUFFER_BYTES (128U * 1024U)
#define ZIP_PREFIX "PPSA34015-app0/"

struct tr_bundle {
    int fd;
    uint64_t archive_size;
    uint64_t total_bytes;
    size_t heap_used;
    int read_errno;
    mz_zip_archive zip;
    mz_uint file_index[TR_ASSET_COUNT];
};

/* Preserve malloc's alignment when attaching allocation size metadata. */
typedef union {
    struct { size_t bytes; } meta;
    max_align_t alignment;
} allocation_header;

static void error_set(char *err, size_t cap, const char *format, ...)
{
    va_list args;
    if (!err || !cap)
        return;
    va_start(args, format);
    vsnprintf(err, cap, format, args);
    va_end(args);
}

static int hash_valid(const char *hex)
{
    size_t i;
    if (!hex || strlen(hex) != 64)
        return 0;
    for (i = 0; i < 64; ++i)
        if (!((hex[i] >= '0' && hex[i] <= '9') ||
              (hex[i] >= 'a' && hex[i] <= 'f')))
            return 0;
    return 1;
}

static void hash_hex(SHA256_CTX *state, char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    BYTE digest[SHA256_BLOCK_SIZE];
    size_t i;
    sha256_final(state, digest);
    for (i = 0; i < sizeof(digest); ++i) {
        hex[2 * i] = digits[digest[i] >> 4];
        hex[2 * i + 1] = digits[digest[i] & 15];
    }
    hex[64] = '\0';
}

static void *bounded_alloc(void *opaque, size_t items, size_t size)
{
    tr_bundle *bundle = opaque;
    allocation_header *header;
    size_t bytes;
    if (size && items > SIZE_MAX / size)
        return NULL;
    bytes = items * size;
    if (bytes > BUNDLE_HEAP_LIMIT - bundle->heap_used ||
        bytes > SIZE_MAX - sizeof(*header))
        return NULL;
    header = malloc(sizeof(*header) + bytes);
    if (!header)
        return NULL;
    header->meta.bytes = bytes;
    bundle->heap_used += bytes;
    return header + 1;
}

static void bounded_free(void *opaque, void *memory)
{
    tr_bundle *bundle = opaque;
    allocation_header *header;
    if (!memory)
        return;
    header = (allocation_header *)memory - 1;
    bundle->heap_used -= header->meta.bytes;
    free(header);
}

static void *bounded_realloc(void *opaque, void *memory,
                             size_t items, size_t size)
{
    tr_bundle *bundle = opaque;
    allocation_header *header;
    size_t bytes, old_bytes;
    if (!memory)
        return bounded_alloc(opaque, items, size);
    if (size && items > SIZE_MAX / size)
        return NULL;
    bytes = items * size;
    header = (allocation_header *)memory - 1;
    old_bytes = header->meta.bytes;
    if (bytes > BUNDLE_HEAP_LIMIT - (bundle->heap_used - old_bytes) ||
        bytes > SIZE_MAX - sizeof(*header))
        return NULL;
    header = realloc(header, sizeof(*header) + bytes);
    if (!header)
        return NULL;
    header->meta.bytes = bytes;
    bundle->heap_used = bundle->heap_used - old_bytes + bytes;
    return header + 1;
}

static size_t archive_read(void *opaque, mz_uint64 offset,
                            void *buffer, size_t bytes)
{
    tr_bundle *bundle = opaque;
    size_t done = 0;
    if (offset > bundle->archive_size ||
        (uint64_t)bytes > bundle->archive_size - offset) {
        bundle->read_errno = EIO;
        return 0;
    }
    while (done < bytes) {
        ssize_t got = pread(bundle->fd, (unsigned char *)buffer + done,
                            bytes - done, (off_t)(offset + done));
        if (got < 0) {
            if (errno == EINTR)
                continue;
            bundle->read_errno = errno;
            break;
        }
        if (!got) {
            bundle->read_errno = EIO;
            break;
        }
        done += (size_t)got;
    }
    return done;
}

static const char *zip_error(tr_bundle *bundle)
{
    const char *text = mz_zip_get_error_string(mz_zip_get_last_error(&bundle->zip));
    return text ? text : "Unknown ZIP error";
}

int bundle_open(const char *zip_path, tr_bundle **out, char *err, size_t err_cap)
{
    tr_bundle *bundle;
    struct stat st;
    mz_uint count, index;
    size_t asset;
    int open_flags = O_RDONLY;
    char filename[BUNDLE_MAX_NAME], expected[BUNDLE_MAX_NAME];

    error_set(err, err_cap, "");
    if (!zip_path || !out) {
        error_set(err, err_cap, "Invalid bundle arguments");
        return -1;
    }
    *out = NULL;
#ifdef O_NOFOLLOW
    open_flags |= O_NOFOLLOW;
#endif
    bundle = calloc(1, sizeof(*bundle));
    if (!bundle) {
        error_set(err, err_cap, "Cannot allocate bundle state");
        return -1;
    }
    bundle->fd = open(zip_path, open_flags);
    if (bundle->fd < 0) {
        error_set(err, err_cap, "Cannot open ZIP: %s", strerror(errno));
        free(bundle);
        return -1;
    }
    if (fstat(bundle->fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        error_set(err, err_cap, "ZIP is not a readable nonempty regular file");
        bundle_close(bundle);
        return -1;
    }
    bundle->archive_size = (uint64_t)st.st_size;
    bundle->zip.m_pRead = archive_read;
    bundle->zip.m_pIO_opaque = bundle;
    bundle->zip.m_pAlloc = bounded_alloc;
    bundle->zip.m_pFree = bounded_free;
    bundle->zip.m_pRealloc = bounded_realloc;
    bundle->zip.m_pAlloc_opaque = bundle;
    for (asset = 0; asset < TR_ASSET_COUNT; ++asset) {
        bundle->file_index[asset] = UINT_MAX;
        if (!tr_assets[asset].path || !hash_valid(tr_assets[asset].sha256hex) ||
            tr_assets[asset].size > UINT64_MAX - bundle->total_bytes) {
            error_set(err, err_cap, "Invalid compiled asset table");
            bundle_close(bundle);
            return -1;
        }
        bundle->total_bytes += tr_assets[asset].size;
    }
    if (!mz_zip_reader_init(&bundle->zip, bundle->archive_size, 0)) {
        error_set(err, err_cap, "Cannot read ZIP directory: %s", zip_error(bundle));
        bundle_close(bundle);
        return -1;
    }
    count = mz_zip_reader_get_num_files(&bundle->zip);
    if (count > BUNDLE_MAX_ENTRIES) {
        error_set(err, err_cap, "ZIP has too many entries");
        bundle_close(bundle);
        return -1;
    }
    for (index = 0; index < count; ++index) {
        mz_uint required = mz_zip_reader_get_filename(&bundle->zip, index, NULL, 0);
        if (!required || required > sizeof(filename) ||
            mz_zip_reader_get_filename(&bundle->zip, index, filename,
                                       sizeof(filename)) != required ||
            strlen(filename) + 1 != required) {
            error_set(err, err_cap, "ZIP contains an invalid entry name");
            bundle_close(bundle);
            return -1;
        }
        for (asset = 0; asset < TR_ASSET_COUNT; ++asset) {
            int chars = snprintf(expected, sizeof(expected), "%s%s",
                                 ZIP_PREFIX, tr_assets[asset].path);
            mz_zip_archive_file_stat file;
            if (chars < 0 || (size_t)chars >= sizeof(expected)) {
                error_set(err, err_cap, "Compiled asset path is too long");
                bundle_close(bundle);
                return -1;
            }
            if (strcmp(filename, expected))
                continue;
            if (bundle->file_index[asset] != UINT_MAX) {
                error_set(err, err_cap, "Duplicate ZIP asset: %s", tr_assets[asset].path);
                bundle_close(bundle);
                return -1;
            }
            if (!mz_zip_reader_file_stat(&bundle->zip, index, &file) ||
                file.m_is_directory || file.m_is_encrypted || !file.m_is_supported ||
                (file.m_method != 0 && file.m_method != MZ_DEFLATED) ||
                file.m_uncomp_size != tr_assets[asset].size) {
                error_set(err, err_cap, "ZIP asset size/type mismatch: %s",
                          tr_assets[asset].path);
                bundle_close(bundle);
                return -1;
            }
            bundle->file_index[asset] = index;
            break;
        }
    }
    for (asset = 0; asset < TR_ASSET_COUNT; ++asset)
        if (bundle->file_index[asset] == UINT_MAX) {
            error_set(err, err_cap, "Missing ZIP asset: %s", tr_assets[asset].path);
            bundle_close(bundle);
            return -1;
        }
    *out = bundle;
    return 0;
}

uint64_t bundle_total_bytes(const tr_bundle *bundle)
{
    return bundle ? bundle->total_bytes : 0;
}

typedef struct {
    int fd;
    int write_errno;
    int invalid_stream;
    uint64_t bytes;
    uint64_t expected;
    SHA256_CTX hash;
    bundle_progress_fn progress;
    void *opaque;
} extraction_state;

static size_t asset_write(void *opaque, mz_uint64 offset,
                           const void *buffer, size_t bytes)
{
    extraction_state *state = opaque;
    size_t done = 0;
    if (offset != state->bytes || state->bytes > state->expected ||
        (uint64_t)bytes > state->expected - state->bytes) {
        state->invalid_stream = 1;
        return 0;
    }
    while (done < bytes) {
        ssize_t written = write(state->fd, (const unsigned char *)buffer + done,
                                bytes - done);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            state->write_errno = errno;
            return done;
        }
        if (!written) {
            state->write_errno = EIO;
            return done;
        }
        done += (size_t)written;
    }
    sha256_update(&state->hash, buffer, bytes);
    state->bytes += bytes;
    if (state->progress)
        state->progress(state->bytes, state->expected, state->opaque);
    return bytes;
}

int bundle_extract_asset(tr_bundle *bundle, size_t asset_index, int output_fd,
                         bundle_progress_fn progress, void *opaque,
                         char *err, size_t err_cap)
{
    extraction_state state;
    char hash[65];
    error_set(err, err_cap, "");
    if (!bundle || asset_index >= TR_ASSET_COUNT || output_fd < 0) {
        error_set(err, err_cap, "Invalid extraction arguments");
        return -1;
    }
    memset(&state, 0, sizeof(state));
    state.fd = output_fd;
    state.expected = tr_assets[asset_index].size;
    state.progress = progress;
    state.opaque = opaque;
    sha256_init(&state.hash);
    bundle->read_errno = 0;
    if (progress)
        progress(0, state.expected, opaque);
    /* flags=0 requires decompression and miniz's complete CRC32 validation. */
    if (!mz_zip_reader_extract_to_callback(&bundle->zip,
             bundle->file_index[asset_index], asset_write, &state, 0)) {
        if (state.write_errno)
            error_set(err, err_cap, "Cannot write %s: %s",
                      tr_assets[asset_index].path, strerror(state.write_errno));
        else if (state.invalid_stream)
            error_set(err, err_cap, "ZIP stream size/order mismatch: %s",
                      tr_assets[asset_index].path);
        else if (bundle->read_errno)
            error_set(err, err_cap, "Cannot read ZIP: %s",
                      strerror(bundle->read_errno));
        else
            error_set(err, err_cap, "ZIP extraction/CRC failed for %s: %s",
                      tr_assets[asset_index].path, zip_error(bundle));
        return -1;
    }
    if (state.bytes != state.expected) {
        error_set(err, err_cap, "Extracted size mismatch: %s", tr_assets[asset_index].path);
        return -1;
    }
    hash_hex(&state.hash, hash);
    if (strcmp(hash, tr_assets[asset_index].sha256hex)) {
        error_set(err, err_cap, "Extracted SHA256 mismatch: %s", tr_assets[asset_index].path);
        return -1;
    }
    return 0;
}

void bundle_close(tr_bundle *bundle)
{
    if (!bundle)
        return;
    if (bundle->zip.m_pState)
        mz_zip_reader_end(&bundle->zip);
    if (bundle->fd >= 0)
        close(bundle->fd);
    free(bundle);
}

int bundle_hash_file(const char *path, uint64_t expected_size,
                     const char *expected_hash, char *err, size_t err_cap)
{
    struct stat st;
    SHA256_CTX state;
    unsigned char *buffer;
    uint64_t total = 0;
    char hash[65];
    int fd, flags = O_RDONLY, result = -1;
    error_set(err, err_cap, "");
    if (!path || !hash_valid(expected_hash)) {
        error_set(err, err_cap, "Invalid file hash arguments");
        return -1;
    }
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    fd = open(path, flags);
    if (fd < 0) {
        error_set(err, err_cap, "Cannot open file for hashing: %s", strerror(errno));
        return -1;
    }
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size != expected_size) {
        error_set(err, err_cap, "Existing file size/type mismatch: %s", path);
        close(fd);
        return -1;
    }
    buffer = malloc(HASH_BUFFER_BYTES);
    if (!buffer) {
        error_set(err, err_cap, "Cannot allocate file hashing buffer");
        close(fd);
        return -1;
    }
    sha256_init(&state);
    for (;;) {
        ssize_t got = read(fd, buffer, HASH_BUFFER_BYTES);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            error_set(err, err_cap, "Cannot read file for hashing: %s", strerror(errno));
            goto done;
        }
        if (!got)
            break;
        if (total > expected_size || (uint64_t)got > expected_size - total) {
            error_set(err, err_cap, "File grew while hashing: %s", path);
            goto done;
        }
        sha256_update(&state, buffer, (size_t)got);
        total += (uint64_t)got;
    }
    if (total != expected_size) {
        error_set(err, err_cap, "File size changed while hashing: %s", path);
        goto done;
    }
    hash_hex(&state, hash);
    if (strcmp(hash, expected_hash)) {
        error_set(err, err_cap, "Existing file SHA256 mismatch: %s", path);
        goto done;
    }
    result = 0;
done:
    free(buffer);
    close(fd);
    return result;
}
