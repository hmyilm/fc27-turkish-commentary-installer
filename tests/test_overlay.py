#!/usr/bin/env python3
"""Exercise overlay transactions with generated data, without game assets.

The production implementation is compiled with the same ten relative paths,
small generated payloads and their independently computed hashes. Console mount
resolution is intentionally outside this host suite.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

PROJECT = Path(__file__).resolve().parents[1]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def tree(path: Path) -> dict:
    if not path.exists():
        return {}
    result = {}
    for item in path.rglob("*"):
        if item.is_file():
            st = item.stat()
            result[str(item.relative_to(path))] = (
                st.st_ino, st.st_mtime_ns, st.st_mode & 0o7777, digest(item.read_bytes())
            )
    return result


def index_for(paths: list[str], contents: list[bytes], *, installed: bool = False) -> bytes:
    names = ["/app0/" + path for path in paths] + ["/app0/eboot.bin"]
    blob, records = bytearray(), bytearray()
    slots = 1
    while slots < len(names) * 2:
        slots *= 2
    table = bytearray(slots * 16)
    for number, name in enumerate(names):
        encoded = name.encode()
        size = len(contents[number]) if installed and number < len(contents) else 0
        if number == len(contents):
            size = 12345
        records += struct.pack("<IIQq", len(blob), len(encoded), size, 1700000000)
        blob += encoded + b"\0"
        value = 1469598103934665603
        for byte in encoded.lower():
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
        value = value or 1
        position = value & (slots - 1)
        while struct.unpack_from("<I", table, position * 16 + 8)[0]:
            position = (position + 1) & (slots - 1)
        struct.pack_into("<QII", table, position * 16, value, number + 1, 0)
    hash_offset = (48 + len(records) + len(blob) + 7) & ~7
    header = struct.pack("<8sIIQQQII", b"AMPRIDX3", 3, 24, len(names),
                         len(blob), hash_offset, 16, slots)
    return header + records + blob + bytes(hash_offset - 48 - len(records) - len(blob)) + table


def write_source(path: Path, index: bytes, *, readonly: bool = True) -> None:
    (path / "Data/Ps5").mkdir(parents=True)
    (path / "sce_sys").mkdir()
    (path / "sce_sys/param.json").write_text(json.dumps({
        "titleId": "PPSA34015", "contentVersion": "01.000.004"
    }))
    (path / "ampr_emu.index").write_bytes(index)
    if readonly:
        for child in path.rglob("*"):
            child.chmod(0o555 if child.is_dir() else 0o444)
        path.chmod(0o555)


def make_writable(path: Path) -> None:
    for child in path.rglob("*"):
        if child.is_dir():
            child.chmod(0o755)
    path.chmod(0o755)


def main() -> int:
    paths = re.findall(r'\{"[^"\n]+", "([^"\n]+)", UINT64_C', (PROJECT / "src/assets.c").read_text())
    assert len(paths) == 10
    contents = [bytes(((j + i * 17) % 251 for j in range(524288 if i == 0 else 4096 + i * 29)))
                for i in range(10)]
    original = index_for(paths, contents)
    installed = index_for(paths, contents, installed=True)
    with tempfile.TemporaryDirectory(prefix="fc27-overlay-") as temporary:
        base = Path(temporary).resolve()
        generated_assets = base / "assets.c"
        rows = [f'    {{"test-{i}", {json.dumps(path)}, UINT64_C({len(data)}), "{digest(data)}"}}'
                for i, (path, data) in enumerate(zip(paths, contents))]
        generated_assets.write_text('#include "assets.h"\nconst tr_asset tr_assets[TR_ASSET_COUNT] = {\n' +
                                    ',\n'.join(rows) + '\n};\n')
        binary = base / "overlay-test"
        sources = ["tests/overlay_host_test.c", "src/bundle.c", "vendor/miniz/miniz.c",
                   "vendor/miniz/miniz_tinfl.c", "vendor/miniz/miniz_zip.c", "vendor/sha256/sha256.c"]
        subprocess.run([os.environ.get("CC", "cc"), "-O2", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-function", "-std=gnu11", "-DMINIZ_NO_STDIO", "-DMINIZ_NO_TIME",
                        "-DMINIZ_NO_ZLIB_APIS", "-DMINIZ_NO_DEFLATE_APIS", "-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES",
                        "-I", str(PROJECT / "src"), "-I", str(PROJECT / "vendor/miniz"),
                        "-I", str(PROJECT / "vendor/sha256"), *(str(PROJECT / p) for p in sources),
                        str(generated_assets), "-o", str(binary)], check=True, timeout=120)
        archive = base / "assets.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
            for path, data in zip(paths, contents):
                output.writestr("PPSA34015-app0/" + path, data)
        count = 0

        def run(label: str, source: Path, overlay: Path | None, *, success: bool,
                check: bool = False, env: dict | None = None, error: str | None = None) -> str:
            nonlocal count
            command = [str(binary), "--game", str(source), "--zip", str(archive)]
            if overlay is not None:
                command += ["--overlay", str(overlay)]
            if check:
                command.append("--check")
            result = subprocess.run(command, text=True, capture_output=True,
                                    env={**os.environ, **(env or {})}, timeout=20)
            text = result.stdout + result.stderr
            assert (result.returncode == 0) == success, (label, result.returncode, text)
            if error:
                assert error in text, (label, text)
            count += 1
            print("PASS " + label, flush=True)
            return text

        source = base / "read-only-source"
        write_source(source, original)
        source_before = tree(source)
        target = base / "backports/PPSA34015"
        run("check mode does not create missing overlay", source, target, success=True, check=True)
        assert not target.parent.exists() and tree(source) == source_before
        target.mkdir(parents=True)
        (target / "fakelib").mkdir()
        unrelated = target / "fakelib/libSceAmpr.sprx"
        unrelated.write_bytes(b"existing unrelated backport bytes")
        (target / "own-config.ini").write_bytes(b"keep this configuration")
        preserved = tree(target)
        run("read-only source installs to existing overlay", source, target, success=True)
        assert tree(source) == source_before
        assert all(tree(target)[name] == value for name, value in preserved.items())
        assert (target / "ampr_emu.index").read_bytes() == installed
        for path, data in zip(paths, contents):
            assert (target / path).read_bytes() == data
        assert not (target / "sce_sys").exists()
        before = tree(target)
        run("overlay reinstall is a true no-op", source, target, success=True)
        run("installed overlay check is a true no-op", source, target, success=True, check=True)
        assert tree(target) == before and tree(source) == source_before
        fresh = base / "new-parent/other/PPSA34015"
        run("install creates missing overlay parents", source, fresh, success=True)
        assert (fresh / "ampr_emu.index").read_bytes() == installed
        assert tree(source) == source_before

        conflict = base / "conflict"
        conflict.mkdir()
        changed = bytearray(original)
        struct.pack_into("<Q", changed, 48 + 10 * 24 + 8, 12346)
        (conflict / "ampr_emu.index").write_bytes(changed)
        conflict_before = tree(conflict)
        run("unrelated index difference rejected", source, conflict, success=False, error="uyusmuyor")
        assert tree(conflict) == conflict_before and tree(source) == source_before

        for where in ("source", "overlay"):
            pack_source = base / f"pack-source-{where}"
            write_source(pack_source, original, readonly=False)
            pack_target = base / f"pack-target-{where}"
            pack_target.mkdir()
            (pack_source if where == "source" else pack_target).joinpath("ampr_assets.index").write_bytes(b"manifest")
            before_source, before_target = tree(pack_source), tree(pack_target)
            run(f"AMPR asset pack in {where} rejected", pack_source, pack_target, success=False,
                error="ampr_assets.index")
            assert tree(pack_source) == before_source and tree(pack_target) == before_target

        for label in ("slot size", "zero slots", "non-power-two slots", "too few slots"):
            bad_source = base / ("bad-" + label.replace(" ", "-"))
            bad = bytearray(original)
            # Keep the old aggregate byte-size equation valid so each fixture
            # specifically exercises the newly enforced runtime constraint.
            hash_offset = struct.unpack_from("<Q", bad, 32)[0]
            if label == "slot size":
                struct.pack_into("<II", bad, 40, 8, 64)
            elif label == "zero slots":
                struct.pack_into("<Q", bad, 32, len(bad))
                struct.pack_into("<I", bad, 44, 0)
            else:
                slots = 31 if label == "non-power-two slots" else 8
                del bad[hash_offset + slots * 16:]
                struct.pack_into("<I", bad, 44, slots)
            write_source(bad_source, bad)
            output = base / (bad_source.name + "-output")
            run(f"invalid AMPR {label} rejected", bad_source, output, success=False, error="sinirlari")
            assert not output.exists()
            make_writable(bad_source)
        bad_source = base / "bad-alignment"
        bad = bytearray(original)
        hash_offset = struct.unpack_from("<Q", bad, 32)[0]
        bad.insert(hash_offset, 0)
        struct.pack_into("<Q", bad, 32, hash_offset + 1)
        write_source(bad_source, bad)
        run("unaligned hash table rejected", bad_source, base / "bad-alignment-output",
            success=False, error="sinirlari")
        make_writable(bad_source)

        # Each rename failure must restore existing target bytes and preserve
        # files outside the ten assets and the index.
        rollback_target = base / "rollback-existing"
        rollback_target.mkdir()
        (rollback_target / "ampr_emu.index").write_bytes(original)
        (rollback_target / "unrelated.bin").write_bytes(b"preserved")
        for path in paths:
            file = rollback_target / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"original target content")
        rollback_before = tree(rollback_target)
        for step in range(1, 23):
            run(f"existing overlay rollback at rename {step}", source, rollback_target,
                success=False, env={"TR_FAIL_COMMIT": str(step)}, error="injected")
            assert tree(rollback_target) == rollback_before
            assert tree(source) == source_before
            assert not list(rollback_target.rglob("*.tr-*"))
            assert not (rollback_target / ".FC27_TR_INSTALL.lock").exists()
        for step in range(1, 12):
            missing = base / f"rollback-new-{step}/PPSA34015"
            run(f"new overlay rollback at rename {step}", source, missing,
                success=False, env={"TR_FAIL_COMMIT": str(step)}, error="injected")
            assert not missing.parent.exists()
            assert tree(source) == source_before

        compatible = base / "compatible-existing-index"
        shutil.copytree(rollback_target, compatible)
        run("compatible existing overlay index is patched", source, compatible, success=True)
        assert (compatible / "ampr_emu.index").read_bytes() == installed
        assert (compatible / "unrelated.bin").read_bytes() == b"preserved"
        assert tree(source) == source_before

        changed_source = base / "changed-source"
        write_source(changed_source, original)
        changed_target = base / "changed-target/PPSA34015"
        run("source changed before commit is rejected", changed_source, changed_target,
            success=False, env={"TR_TEST_SOURCE_CHANGE": "1"}, error="degisti")
        assert not changed_target.parent.exists()
        make_writable(changed_source)

        final_target = base / "final-verification"
        shutil.copytree(rollback_target, final_target)
        (final_target / paths[0]).write_bytes(contents[0])
        run("final verification includes preexisting correct assets", source, final_target,
            success=False, env={"TR_TEST_CORRUPT_UNCHANGED": "1"}, error="SHA256 mismatch")
        assert (final_target / "unrelated.bin").read_bytes() == b"preserved"
        for path in paths[1:]:
            assert (final_target / path).read_bytes() == b"original target content"
        assert (final_target / "ampr_emu.index").read_bytes() == original

        run("source and overlay cannot coincide", source, source, success=False, error="ayri olmali")
        run("overlay cannot be a source child", source, source / "overlay", success=False, error="ayri olmali")
        aliased = base / "aliased-target"
        aliased.symlink_to(target, target_is_directory=True)
        run("symlink overlay rejected", source, aliased, success=False, error="normal klasor")

        cancellation = subprocess.run([str(binary), "cancel-bundle", str(archive), str(base / "cancelled.bin")],
                                      capture_output=True, text=True, timeout=20)
        assert cancellation.returncode == 0, cancellation.stdout + cancellation.stderr
        count += 1
        print("PASS cooperative extraction " + cancellation.stdout.strip(), flush=True)

        folder = base / "folder-mode"
        write_source(folder, original, readonly=False)
        run("ordinary folder install still works", folder, None, success=True)
        assert (folder / "ampr_emu.index").read_bytes() == installed
        for path, data in zip(paths, contents):
            assert (folder / path).read_bytes() == data
        folder_before = tree(folder)
        run("ordinary folder reinstall is a no-op", folder, None, success=True)
        assert tree(folder) == folder_before
        make_writable(source)
        print(f"PASS {count} overlay/core cases; generated assets only, no PS5 mount behavior proven.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
