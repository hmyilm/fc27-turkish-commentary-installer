#!/usr/bin/env python3
"""Host integration tests using an external, privately held asset ZIP/index.

No game assets are bundled with this test. The installer must already be built
for the host. A temporary game folder is removed when the test completes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import zipfile


PROJECT = Path(__file__).resolve().parents[1]


def index_entries(data: bytes) -> dict[str, tuple[int, int]]:
    if len(data) < 48:
        raise AssertionError("Index header missing")
    magic, version, entry_size, count, path_bytes, hash_offset, slot_size, slot_count = (
        struct.unpack_from("<8sIIQQQII", data)
    )
    assert magic == b"AMPRIDX3" and version == 3 and entry_size == 24
    paths_start = 48 + count * entry_size
    assert paths_start + path_bytes <= hash_offset
    assert hash_offset + slot_size * slot_count == len(data)
    entries = {}
    for number in range(count):
        offset = 48 + number * entry_size
        path_offset, path_length, size, _mtime = struct.unpack_from("<IIQq", data, offset)
        assert path_offset + path_length <= path_bytes
        path = data[paths_start + path_offset:paths_start + path_offset + path_length]
        name = path.decode("utf-8").casefold()
        assert name not in entries
        entries[name] = (offset, size)
    return entries


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def snapshot(game: Path, assets: list[dict]) -> dict:
    result = {}
    for relative in [a["path"] for a in assets] + ["ampr_emu.index"]:
        path = game / relative
        st = path.stat()
        result[relative] = (st.st_size, st.st_ino, st.st_mtime_ns, file_hash(path))
    return result


def write_param(game: Path, version: str, title: str = "PPSA34015") -> None:
    path = game / "sce_sys/param.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"titleId": title, "contentVersion": version}), encoding="utf-8")


def prepare_fixture(game: Path, original: bytes, assets: list[dict], archive: zipfile.ZipFile) -> None:
    game.mkdir(parents=True)
    (game / "ampr_emu.index").write_bytes(original)
    entries = index_entries(original)
    for item in assets:
        key = ("/app0/" + item["path"]).casefold()
        assert key in entries
        _offset, old_size = entries[key]
        assert old_size in (0, item["size"])
        target = game / item["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if old_size == 0:
            target.write_bytes(b"")
        else:
            with archive.open("PPSA34015-app0/" + item["path"]) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output, 1024 * 1024)
            assert file_hash(target) == item["sha256"]
        target.chmod(0o666)
    write_param(game, "01.000.003")


def run_installer(binary: Path, game: Path, bundle: Path, *, env: dict | None = None,
                  expect_success: bool, label: str, check_only: bool = False) -> subprocess.CompletedProcess:
    # Host CLI is deliberately explicit; no PS5 autodiscovery is used here.
    command = [str(binary), "--game", str(game), "--zip", str(bundle)]
    if check_only:
        command.append("--check")
    started = time.monotonic()
    result = subprocess.run(command, capture_output=True, text=True,
                            env={**os.environ, **(env or {})}, timeout=300)
    output = result.stdout + result.stderr
    assert (result.returncode == 0) == expect_success, (
        f"{label}: unexpected exit {result.returncode}\n{output}"
    )
    print(f"PASS {label} ({time.monotonic() - started:.2f}s)", flush=True)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=PROJECT / "build/FC27_TR_KUR_host")
    parser.add_argument("--zip", required=True, type=Path, help="Existing external Turkish asset ZIP")
    parser.add_argument("--index", required=True, type=Path, help="Existing missing-TR AMPRIDX3 index; not included in the repository")
    parser.add_argument("--temp-dir", type=Path, help="Parent folder with at least 2 GB available")
    parser.add_argument("--skip-rollback", action="store_true", help="Only when host fault injection was disabled")
    args = parser.parse_args()
    if not args.binary.is_file() or not args.zip.is_file() or args.index is None or not args.index.is_file():
        parser.error("Built host binary, external ZIP, and an external AMPR index are required")
    binary, bundle = args.binary.resolve(), args.zip.resolve()
    original = args.index.read_bytes()
    entries = index_entries(original)
    with zipfile.ZipFile(bundle) as archive:
        manifest = json.loads(archive.read("DOSYA_LISTESI_SHA256.json"))
        assert manifest["title_id"] == "PPSA34015"
        assets = manifest["files"]
        assert len(assets) == 10
        expected = bytearray(original)
        changed_offsets = []
        for item in assets:
            offset, old_size = entries[("/app0/" + item["path"]).casefold()]
            assert old_size in (0, item["size"])
            if not old_size:
                changed_offsets.append(offset + 8)
                struct.pack_into("<Q", expected, offset + 8, item["size"])
        assert len(changed_offsets) == 6, "Use a missing-TR index with the six original zero-size records"
        expected = bytes(expected)
        with tempfile.TemporaryDirectory(prefix="tr-installer-test-", dir=args.temp_dir) as temporary:
            base = Path(temporary)
            game = base / "PPSA34015-app0"
            prepare_fixture(game, original, assets, archive)
            before = snapshot(game, assets)

            run_installer(binary, game, bundle, expect_success=True,
                          label="003 check mode performs no writes", check_only=True)
            assert snapshot(game, assets) == before, "Check mode modified game files"

            write_param(game, "01.000.003", "PPSA00000")
            run_installer(binary, game, bundle, expect_success=False, label="wrong title rejected")
            assert snapshot(game, assets) == before, "Wrong title modified game files"
            write_param(game, "01.000.003")

            damaged = bytearray(original)
            bad_item = assets[0]
            bad_offset = entries[("/app0/" + bad_item["path"]).casefold()][0] + 8
            struct.pack_into("<Q", damaged, bad_offset, bad_item["size"] + 1)
            (game / "ampr_emu.index").write_bytes(damaged)
            bad_before = snapshot(game, assets)
            run_installer(binary, game, bundle, expect_success=False, label="incompatible nonzero index size rejected")
            assert snapshot(game, assets) == bad_before, "Incompatible index modified game files"
            (game / "ampr_emu.index").write_bytes(original)

            if not args.skip_rollback:
                rollback_before = snapshot(game, assets)
                result = run_installer(binary, game, bundle,
                    env={"TR_FAIL_COMMIT": "7"}, expect_success=False,
                    label="injected mid-commit failure rejected")
                assert "injected" in (result.stdout + result.stderr).casefold(), "Fault injection did not trigger"
                after_rollback = snapshot(game, assets)
                for path, old in rollback_before.items():
                    assert after_rollback[path][0] == old[0] and after_rollback[path][3] == old[3], (
                        f"Rollback did not restore bytes: {path}"
                    )
                assert (game / "ampr_emu.index").read_bytes() == original
                print("PASS rollback restored all ten original asset bytes and complete index", flush=True)

            run_installer(binary, game, bundle, expect_success=True, label="003 full installation")
            for item in assets:
                target = game / item["path"]
                assert target.stat().st_size == item["size"]
                assert file_hash(target) == item["sha256"]
            actual = (game / "ampr_emu.index").read_bytes()
            assert actual == expected, "Index changed outside the six permitted size fields"
            allowed = {position for offset in changed_offsets for position in range(offset, offset + 8)}
            assert all(position in allowed for position, (old, new) in enumerate(zip(original, actual)) if old != new)
            print("PASS all ten full file SHA256 values and only six u64 index size fields", flush=True)

            installed = snapshot(game, assets)
            run_installer(binary, game, bundle, expect_success=True, label="003 repeat installation/noop")
            assert snapshot(game, assets) == installed, "Repeat install rewrote existing correct files"
            write_param(game, "01.000.004")
            run_installer(binary, game, bundle, expect_success=True, label="004 accepted/noop")
            assert snapshot(game, assets) == installed, "004 noop rewrote existing correct files"
    print("PASS host integration suite; this does not prove 003 runtime game compatibility", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
