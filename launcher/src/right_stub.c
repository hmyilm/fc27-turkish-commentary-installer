/* SPDX-License-Identifier: GPL-3.0-or-later
 * Inert auxiliary module authored for this homebrew package.
 * No Sony-provided executable bytes or SDK runtime are embedded here.
 * It exports ordinary module lifecycle symbols and imports no libraries.
 * Console compatibility of the packaged module must be tested separately.
 */
int module_start(unsigned long argc, const void *argv) {
    (void)argc;
    (void)argv;
    return 0;
}

int module_stop(unsigned long argc, const void *argv) {
    (void)argc;
    (void)argv;
    return 0;
}

int _init(void) { return 0; }
int _fini(void) { return 0; }
