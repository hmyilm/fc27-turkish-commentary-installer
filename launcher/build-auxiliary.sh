#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build our own inert auxiliary module; no console SDK libraries are linked.
set -eu
task_launcher_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -z "${PS5_PAYLOAD_SDK:-}" ] || [ ! -x "$PS5_PAYLOAD_SDK/bin/prospero-clang" ]; then
    printf '%s\n' 'Set PS5_PAYLOAD_SDK to the PS5 payload SDK directory.' >&2
    exit 2
fi
mkdir -p "$task_launcher_dir/build"
"$PS5_PAYLOAD_SDK/bin/prospero-clang" -O2 -Wall -Wextra -Werror -std=gnu11 \
    -ffreestanding -fno-builtin -fno-stack-protector -fPIC \
    -fno-asynchronous-unwind-tables -fno-unwind-tables -nodefaultlibs \
    "-ffile-prefix-map=$task_launcher_dir=/src/fc27-tr-installer/launcher" \
    -c "$task_launcher_dir/src/right_stub.c" -o "$task_launcher_dir/build/right_stub.o"
"$PS5_PAYLOAD_SDK/bin/prospero-lld" --shared --build-id=none \
    -o "$task_launcher_dir/build/right_stub.elf" "$task_launcher_dir/build/right_stub.o"
file "$task_launcher_dir/build/right_stub.elf"
