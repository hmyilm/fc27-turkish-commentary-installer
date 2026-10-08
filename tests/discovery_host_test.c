/* SPDX-License-Identifier: GPL-3.0-or-later
 * Exercise the production discovery implementation using synthetic metadata.
 * No game assets, package data or console API are required.
 */
#define TR_HOST_TEST
#define TR_DISCOVERY_TEST
#define main installer_main_for_discovery_test
#include "../src/installer.c"
#undef main

int main(int argc, char **argv) {
    char found[TR_PATH] = {0}, err[TR_ERR] = {0};
    int result = 0;
    if (argc == 3 && !strcmp(argv[1], "storage")) {
        char storage[32];
        if (!game_storage_root(argv[2], storage)) {
            fprintf(stderr, "Unsupported game storage.\n");
            return 1;
        }
        printf("%s\n", storage);
        return 0;
    } else if (argc == 3 && !strcmp(argv[1], "manual")) {
        int version003 = 0;
        result = copy_path(found, argv[2], err) || canonical_game(found, err) ||
                 validate_param(found, &version003, err);
    } else if (argc == 5 && !strcmp(argv[1], "limits")) {
        struct stat st;
        if (stat(argv[2], &st))
            return 2;
        game_search search = {0, 0, (size_t)strtoul(argv[3], NULL, 10),
                              (size_t)strtoul(argv[4], NULL, 10),
                              DISCOVERY_MAX_DEPTH, st.st_dev, found};
        result = walk_game_directories(argv[2], 0, &search, err);
    } else if (argc >= 3 && !strcmp(argv[1], "scan")) {
        for (int i = 2; i < argc && !result; ++i)
            result = scan_game_root(argv[i], found, err);
    } else {
        fprintf(stderr, "usage: discovery-test scan ROOT... | manual GAME | limits ROOT DIRS ENTRIES\n");
        return 2;
    }
    if (result) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (!found[0]) {
        fprintf(stderr, "No matching game folder.\n");
        return 3;
    }
    printf("%s\n", found);
    return 0;
}
