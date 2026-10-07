#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Yerel AMPRIDX3 kopyasındaki Türkçe dosya boyutlarını bu ZIP'e göre düzeltir."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile


def patch_index(original, files):
    if len(original) < 48:
        raise ValueError('AMPR indeks başlığı eksik.')
    magic, version, entry_size, count, path_bytes, hash_offset, slot_size, slot_count = struct.unpack_from('<8sIIQQQII', original)
    if magic != b'AMPRIDX3' or version != 3 or entry_size != 24:
        raise ValueError('Bu araç yalnızca AMPRIDX3 / sürüm 3 / kayıt boyutu 24 indeksini destekler.')
    blob_start = 48 + count * entry_size
    if count > 100_000 or blob_start + path_bytes > hash_offset or hash_offset + slot_size * slot_count != len(original):
        raise ValueError('AMPR indeksinin sınırları tutarlı değil.')
    found = {}
    for i in range(count):
        offset = 48 + i * entry_size
        po, pl, size, mtime = struct.unpack_from('<IIQq', original, offset)
        if po + pl > path_bytes:
            raise ValueError('İndeksteki dosya yolu sınır dışı.')
        name = original[blob_start + po:blob_start + po + pl].decode('utf-8').casefold()
        if name in found:
            raise ValueError('Tekrar eden indeks yolu; işlem yapılmadı.')
        found[name] = (offset, size)
    data = bytearray(original)
    changes = []
    for item in files:
        key = ('/app0/' + item['path']).casefold()
        if key not in found:
            raise ValueError('Gerekli Türkçe yolu bu indekste yok: ' + item['path'])
        offset, old = found[key]
        new = item['size']
        if old not in (0, new):
            raise ValueError('Mevcut Türkçe dosya boyutu bu paketle uyuşmuyor: ' + item['path'])
        if old != new:
            struct.pack_into('<Q', data, offset + 8, new)
            changes.append({'path': item['path'], 'old_size': old, 'new_size': new, 'record_offset': offset})
    # Dosya yolları, tarihleri, kayıt sırası ve hash tablosu aynen korunur.
    return bytes(data), changes


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', required=True, type=Path, help='PS5 oyun klasöründen alınan ampr_emu.index')
    p.add_argument('--param', required=True, type=Path, help='Aynı PS5 oyun klasörünün sce_sys/param.json dosyası')
    p.add_argument('--paket', required=True, type=Path, help='Türkçe spiker ZIP dosyası')
    p.add_argument('--output', type=Path, default=Path('ampr_emu_TR.index'))
    args = p.parse_args()
    if args.output.resolve() == args.index.resolve() or args.output.exists():
        raise ValueError('Çıktı için yeni bir dosya adı kullanın; orijinal indeksin üzerine yazılmaz.')
    target = json.loads(args.param.read_text(encoding='utf-8-sig'))
    allowed_versions = ('01.000.003', '01.000.004')
    if target.get('titleId') != 'PPSA34015' or target.get('contentVersion') not in allowed_versions:
        raise ValueError('Hedef oyun PPSA34015 olmalı; kabul edilen sürümler 01.000.003 ve 01.000.004.')
    if target['contentVersion'] == '01.000.003':
        print('Hedef sürüm 01.000.003 kabul edildi. Bu sürümde oyun içi uyumluluk henüz doğrulanmadı.')
    with zipfile.ZipFile(args.paket) as archive:
        m = json.loads(archive.read('DOSYA_LISTESI_SHA256.json'))
        if m.get('title_id') != 'PPSA34015' or m.get('version') != '01.000.004' or len(m['files']) != 10:
            raise ValueError('Beklenen PPSA34015 v01.000.004 Türkçe dosya listesi bulunamadı.')
        for item in m['files']:
            h = hashlib.sha256()
            size = 0
            with archive.open('PPSA34015-app0/' + item['path']) as f:
                while block := f.read(1024 * 1024):
                    h.update(block)
                    size += len(block)
            if size != item['size'] or h.hexdigest() != item['sha256']:
                raise ValueError('ZIP dosyası doğrulanamadı: ' + item['path'])
    output, changes = patch_index(args.index.read_bytes(), m['files'])
    with args.output.open('xb') as f:
        f.write(output)
    print('Türkçe dosyaları doğrulandı. Güncellenen kayıt sayısı:', len(changes))
    for item in changes:
        print(item['path'], item['old_size'], '->', item['new_size'])
    print('Yeni indeks:', args.output)
    print('PS5 üzerinde değişiklik yapılmadı. Kurulum notundaki aktarım adımlarını izleyin.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, UnicodeError, struct.error, zipfile.BadZipFile) as e:
        print('HATA:', e, file=sys.stderr)
        sys.exit(1)
