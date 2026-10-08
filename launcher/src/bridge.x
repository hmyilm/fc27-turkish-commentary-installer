/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from PS5 payload SDK v0.43 samples/install_app/eboot.x.
 * Copyright (C) 2025 John Tornblom.
 */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
PHDRS {
    ph_text PT_LOAD FLAGS(0x5);
    ph_rodata PT_LOAD FLAGS(0x4);
    ph_data PT_LOAD FLAGS(0x6);
    ph_sce_procparam 0x61000001 FLAGS(0x4);
}
SECTIONS {
    . = CONSTANT(MAXPAGESIZE);
    .text : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.text .text.*)
    } : ph_text
    .rodata : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.rodata .rodata.*)
    } : ph_rodata
    .data : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.data .data.*)
    } : ph_data
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } : ph_data
    .sce_process_param : ALIGN(32) {
        KEEP(*(.sce_process_param))
    } : ph_sce_procparam
    /DISCARD/ : {
        *(.eh_frame .eh_frame_hdr)
        *(.comment)
    }
}
