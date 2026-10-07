/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TR_ASSETS_H
#define TR_ASSETS_H
#include <stdint.h>
typedef struct tr_asset {
    const char *name;
    const char *path;
    uint64_t size;
    char sha256hex[65];
} tr_asset;
enum { TR_ASSET_COUNT = 10 };
extern const tr_asset tr_assets[TR_ASSET_COUNT];
#endif
