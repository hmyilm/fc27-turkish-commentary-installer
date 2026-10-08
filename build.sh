#!/bin/sh
set -eu
task_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
task_output="$task_source_dir/build/FC27_TR_KUR.elf"
task_host_define=
if [ "${1:-}" = "--host" ]; then
    task_sdk_cc=${CC:-cc}
    task_host_define=-DTR_HOST_TEST
    task_output="$task_source_dir/build/FC27_TR_KUR_host"
    task_object_dir="$task_source_dir/build/host-objects"
elif [ "$#" -ne 0 ]; then
    printf '%s\n' 'Usage: sh ./build.sh [--host]' >&2
    exit 2
else
    if [ -z "${PS5_PAYLOAD_SDK:-}" ] || [ ! -x "$PS5_PAYLOAD_SDK/bin/prospero-clang" ]; then
        printf '%s\n' 'Set PS5_PAYLOAD_SDK to the PS5 payload SDK directory (containing bin/prospero-clang).' >&2
        exit 2
    fi
    task_sdk_cc="$PS5_PAYLOAD_SDK/bin/prospero-clang"
    task_object_dir="$task_source_dir/build/ps5-objects"
fi
mkdir -p "$task_object_dir"
set --
for task_relative in src/installer.c src/assets.c src/bundle.c \
                     vendor/miniz/miniz.c vendor/miniz/miniz_tinfl.c \
                     vendor/miniz/miniz_zip.c vendor/sha256/sha256.c; do
    task_name=$(basename "$task_relative" .c)
    task_object="$task_object_dir/$task_name.o"
    task_extra_warning=
    case "$task_relative" in
        vendor/miniz/*) task_extra_warning=-Wno-unused-function ;;
    esac
    "$task_sdk_cc" -O2 -Wall -Wextra -std=gnu11 $task_host_define $task_extra_warning \
        "-ffile-prefix-map=$task_source_dir=." \
        -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ZLIB_APIS \
        -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES \
        -I "$task_source_dir/src" -I "$task_source_dir/vendor/miniz" \
        -I "$task_source_dir/vendor/sha256" \
        -c "$task_source_dir/$task_relative" -o "$task_object"
    set -- "$@" "$task_object"
done
"$task_sdk_cc" "$@" -o "$task_output"
file "$task_output"
