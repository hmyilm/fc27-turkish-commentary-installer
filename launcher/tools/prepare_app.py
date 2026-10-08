#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a code-only launcher root from our bridge and auxiliary ELF."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import zlib


def png_chunk(name, body):
    return struct.pack('>I', len(body)) + name + body + struct.pack(
        '>I', zlib.crc32(name + body) & 0xffffffff)


def make_icon():
    # Original TR pixel lettering on a blue background, generated without fonts.
    glyphs = ['11111', '00100', '00100', '00100', '00100', '00100', '00100'], [
        '11110', '10001', '10001', '11110', '10100', '10010', '10001']
    pixels = bytearray()
    for y in range(512):
        pixels.append(0)
        for x in range(512):
            color = (8, 24, 40, 255)
            if 24 <= x < 488 and 24 <= y < 488:
                color = (5, 64 + y // 5, 113 + y // 5, 255)
            if 45 <= x < 467 and 45 <= y < 467:
                color = (8, 32, 49, 255)
            for i, glyph in enumerate(glyphs):
                gx = (x - (81 + i * 186)) // 31
                gy = (y - 147) // 31
                if 0 <= gx < 5 and 0 <= gy < 7 and glyph[gy][gx] == '1':
                    color = (245, 250, 253, 255)
            pixels.extend(color)
    return (b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack(
        '>IIBBBBB', 512, 512, 8, 6, 0, 0, 0)) + png_chunk(
        b'IDAT', zlib.compress(pixels, 9)) + png_chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bridge', required=True, type=Path)
    parser.add_argument('--right', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Use a fresh output folder.')
    for source in [args.bridge, args.right]:
        header = source.read_bytes()[:64]
        if len(header) != 64 or header[:7] != b'\x7fELF\x02\x01\x01':
            parser.error('Expected a raw little-endian ELF64 module: ' + str(source))
        if struct.unpack_from('<H', header, 0x12)[0] != 62:
            parser.error('Expected x86-64 module: ' + str(source))
    args.output.mkdir(parents=True)
    sce_sys = args.output / 'sce_sys'
    about = sce_sys / 'about'
    about.mkdir(parents=True)
    shutil.copyfile(args.bridge, args.output / 'eboot.bin')
    shutil.copyfile(args.right, about / 'right.sprx')
    param = {
        'applicationCategoryType': 0,
        'applicationDrmType': 'free',
        'contentId': 'UP9000-PPSA99027_00-FCTRINSTALLER001',
        'titleId': 'PPSA99027',
        'contentVersion': '01.000.000',
        'masterVersion': '01.00',
        # Match the pinned packager's minimum normalized metadata version.
        'requiredSystemSoftwareVersion': '0x0200000000000000',
        'sdkVersion': '0x0200000000000000',
        'localizedParameters': {
            'defaultLanguage': 'en-US',
            'en-US': {'titleName': 'FC27 Türkçe Spiker Kurucu'},
            'tr-TR': {'titleName': 'FC27 Türkçe Spiker Kurucu'},
        },
    }
    (sce_sys / 'param.json').write_text(json.dumps(
        param, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (sce_sys / 'icon0.png').write_bytes(make_icon())
    print('Prepared code-only application root:', args.output)


if __name__ == '__main__':
    main()
