#!/bin/sh
# Build an installed-application stub; keep the payload itself unchanged.
set -eu
task_project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -z "${PS5_PAYLOAD_SDK:-}" ] || [ ! -x "$PS5_PAYLOAD_SDK/bin/prospero-clang" ]; then
    printf '%s\n' 'Set PS5_PAYLOAD_SDK to the PS5 payload SDK directory.' >&2
    exit 2
fi
task_payload=${TR_INSTALLER_ELF:-"$task_project_dir/build/FC27_TR_KUR.elf"}
task_bridge_build="$task_project_dir/launcher/build"
mkdir -p "$task_bridge_build"
"${PYTHON:-python3}" - "$task_payload" "$task_bridge_build/installer_payload.h" \
    "$task_bridge_build/embedded-payload.json" <<'PY'
import hashlib
import json
from pathlib import Path
import struct
import sys
source = Path(sys.argv[1])
payload = source.read_bytes()
if len(payload) < 64 or payload[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', payload, 18)[0] != 62:
    raise SystemExit('The embedded installer must be an x86-64 ELF64 payload.')
header = Path(sys.argv[2])
with header.open('w', encoding='ascii') as f:
    f.write('/* Generated at build time; contains the existing installer ELF. */\n')
    f.write('static const unsigned char installer_payload[] = {\n')
    for start in range(0, len(payload), 16):
        f.write('    ' + ','.join(f'0x{value:02x}' for value in payload[start:start+16]) + ',\n')
    f.write('};\nstatic const unsigned long installer_payload_size = sizeof(installer_payload);\n')
Path(sys.argv[3]).write_text(json.dumps({'file': source.name, 'size': len(payload),
                                     'sha256': hashlib.sha256(payload).hexdigest()}, indent=2) + '\n')
PY
"$PS5_PAYLOAD_SDK/bin/prospero-clang" -O2 -Wall -Wextra -Werror -std=gnu11 \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -nodefaultlibs -I "$task_bridge_build" \
    -c "$task_project_dir/launcher/src/bridge.c" -o "$task_bridge_build/bridge.o"
"$PS5_PAYLOAD_SDK/bin/prospero-lld" --static --build-id=none \
    -T "$task_project_dir/launcher/src/bridge.x" \
    -o "$task_bridge_build/bridge.elf" "$task_bridge_build/bridge.o"
file "$task_bridge_build/bridge.elf"
# SDK's make_fself.py emits a PS4 fake SELF. It is optional diagnostic output;
# a native PS5 package builder must wrap bridge.elf as a PS5 fake SELF itself.
if [ "${TR_BUILD_PS4_FSELF_DIAGNOSTIC:-0}" = 1 ]; then
    "${PYTHON:-python3}" "$PS5_PAYLOAD_SDK/samples/install_app/make_fself.py" \
        --ptype fake --paid 0x3800000000000022 \
        --app-version 0x07590001 --fw-version 0x07590001 \
        "$task_bridge_build/bridge.elf" "$task_bridge_build/bridge-PS4-diagnostic.bin"
fi
