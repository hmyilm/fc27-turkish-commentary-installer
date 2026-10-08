#!/usr/bin/env python3
"""Build and exercise actual installer discovery using tiny temporary fixtures.

This checks path/metadata selection, not game assets or PS5 runtime behavior.
No external dependencies or game files are needed.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]


def make_game(path: Path, *, title: str = "PPSA34015", version: str = "01.000.004") -> Path:
    (path / "sce_sys").mkdir(parents=True)
    (path / "Data/Ps5").mkdir(parents=True)
    (path / "sce_sys/param.json").write_text(
        json.dumps({"titleId": title, "contentVersion": version}), encoding="utf-8"
    )
    # This deliberately is not a full AMPR index: discovery only requires a
    # regular index file. The existing install tests cover index integrity.
    (path / "ampr_emu.index").write_bytes(b"synthetic discovery fixture")
    return path


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="fc27-discovery-") as temporary:
        base = Path(temporary).resolve()
        binary = base / "discovery-test"
        sources = [
            "tests/discovery_host_test.c", "src/assets.c", "src/bundle.c",
            "vendor/miniz/miniz.c", "vendor/miniz/miniz_tinfl.c",
            "vendor/miniz/miniz_zip.c", "vendor/sha256/sha256.c",
        ]
        subprocess.run([
            os.environ.get("CC", "cc"), "-O2", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-function", "-std=gnu11", "-DMINIZ_NO_STDIO", "-DMINIZ_NO_TIME",
            "-DMINIZ_NO_ZLIB_APIS", "-DMINIZ_NO_DEFLATE_APIS",
            "-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES", "-I", str(PROJECT / "src"),
            "-I", str(PROJECT / "vendor/miniz"), "-I", str(PROJECT / "vendor/sha256"),
            *(str(PROJECT / name) for name in sources), "-o", str(binary),
        ], check=True, timeout=120)
        count = 0

        def run(label: str, *args: object, expected: Path | None = None,
                error: str | None = None, absent: bool = False) -> None:
            nonlocal count
            result = subprocess.run([str(binary), *(str(arg) for arg in args)],
                                    capture_output=True, text=True, timeout=10)
            if expected is not None:
                assert result.returncode == 0, (label, result.stdout, result.stderr)
                assert result.stdout.strip() == str(expected.resolve()), (label, result.stdout)
            elif absent:
                assert result.returncode == 3, (label, result.stdout, result.stderr)
            else:
                assert result.returncode == 1, (label, result.stdout, result.stderr)
                assert error and error in result.stderr, (label, result.stderr)
            count += 1
            print(f"PASS {label}", flush=True)

        # These are the same path guards called by the native PS5 build.
        for usb in range(8):
            root = Path(f"/mnt/usb{usb}")
            run(f"native USB{usb} root allowed", "storage", root, expected=root)
            run(f"native USB{usb} arbitrary child allowed", "storage", root / "Games/FC27", expected=root)
        run("native data path allowed", "storage", "/data/games/FC27", expected=Path("/data"))
        for unsupported in ("/mnt/sandbox/PPSA34015/app0", "/mnt/pfs/game", "/app0",
                            "/mnt/usb8/game", "/mnt/usb00/game", "/mnt/usb0-image", "/database/game"):
            run(f"native image or unsupported path rejected {unsupported}", "storage", unsupported,
                error="Unsupported game storage")

        # Invoke actual production discover_game root selection. The C test
        # hook only prefixes absolute roots with this temporary filesystem.
        for number, relative in enumerate((
            "FC27", "My own library/Football/Any folder name",
            "etaHEN/games/a/b/c/d/FC27", "OnionHEN/games/a/b/c/d/FC27",
            "/".join(["nested"] * 31 + ["FC27"]),
        )):
            root = base / f"production-data-{number}"
            game = make_game(root / "data" / relative)
            run(f"production /data search layout {number}", "discover", root, expected=game)

        root = base / "production-data-depth-bound"
        game = make_game(root / "data" / Path(*(["nested"] * 32 + ["FC27"])))
        run("production /data depth33 excluded", "discover", root, error="bulunamadi")
        run("manual path beyond /data search depth allowed", "manual", game, expected=game)

        for usb in range(8):
            root = base / f"production-usb-{usb}"
            game = make_game(root / f"mnt/usb{usb}" / "one/two/three/FC27")
            run(f"production USB{usb} depth4 retained", "discover", root, expected=game)
        root = base / "production-usb-depth-bound"
        make_game(root / "mnt/usb0/one/two/three/four/FC27")
        run("production USB depth5 still excluded", "discover", root, error="bulunamadi")

        root = base / "production-data-duplicates"
        make_game(root / "data/FC27")
        make_game(root / "data/Other/Archive/FC27")
        run("production internal duplicates require choice", "discover", root, error="Birden fazla")
        root = base / "production-cross-storage-duplicates"
        make_game(root / "data/My FC27")
        make_game(root / "mnt/usb7/FC27")
        run("production data and USB duplicates require choice", "discover", root, error="Birden fazla")

        root = base / "production-outside-data"
        make_game(root / "Other/FC27")
        run("production ignores other filesystem roots", "discover", root, error="bulunamadi")
        root = base / "production-data-game-boundary"
        game = make_game(root / "data/FC27")
        make_game(game / "Data/Ps5/also-looks-like-a-game")
        run("production broad /data scan stops at game metadata", "discover", root, expected=game)

        root = base / "production-data-budget"
        data = root / "data"
        data.mkdir(parents=True)
        for number in range(4096):
            (data / f"folder-{number}").mkdir()
        run("production /data directory budget fails safely", "discover", root, error="arama sinirina")

        # Common layouts, wrapper folders and arbitrary casing/names all use
        # the same production traversal; no name allowlist is required.
        layouts = [
            ("root", "."),
            ("arbitrary basename", "EA Sports FC 27 EUR"),
            ("games folder", "games/Football"),
            ("PS5 folder", "PS5/FC27"),
            ("etaHEN folder", "etaHEN/games/PPSA34015-app0"),
            ("OnionHEN folder", "OnionHEN/games/FC27"),
            ("mixed case folders", "ONIONhen/GAMES/FC 27"),
            ("four levels including wrapper", "MyGames/PS5/FC27/app0"),
        ]
        for number, (label, relative) in enumerate(layouts):
            root = base / f"layout-{number}"
            game = make_game(root / relative)
            run(label, "scan", root, expected=game)

        root = base / "depth"
        game = make_game(root / "one/two/three/four/five")
        run("depth five excluded from automatic scan", "scan", root, absent=True)
        run("manual path accepts deeper arbitrary name", "manual", game, expected=game)
        root = base / "duplicate"
        make_game(root / "FC27-A")
        make_game(root / "games/FC27-B", version="01.000.003")
        run("multiple compatible folders require choice", "scan", root, error="Birden fazla")
        run("manual path resolves duplicate choice", "manual", root / "FC27-A", expected=root / "FC27-A")
        other_root = base / "another-device"
        make_game(other_root / "FC27")
        run("duplicates across roots require choice", "scan", base / "layout-1", other_root,
            error="Birden fazla")
        run("same canonical root twice is one game", "scan", base / "layout-1", base / "layout-1",
            expected=base / "layout-1/EA Sports FC 27 EUR")

        root = base / "wrong-title"
        wrong = make_game(root / "PPSA34015-app0", title="PPSA00000")
        run("name alone cannot select wrong game", "scan", root, absent=True)
        run("manual wrong title still rejected", "manual", wrong, error="Hedef PPSA34015")
        unsupported = make_game(base / "unsupported/FC27", version="01.000.005")
        run("unsupported version ignored", "scan", unsupported.parent, absent=True)
        run("manual unsupported version rejected", "manual", unsupported, error="Hedef PPSA34015")
        missing = make_game(base / "missing-index/FC27")
        (missing / "ampr_emu.index").unlink()
        run("missing index not a candidate", "scan", missing.parent, absent=True)
        run("manual missing index rejected", "manual", missing, error="ampr_emu.index")
        missing_data = make_game(base / "missing-data/FC27")
        (missing_data / "Data/Ps5").rmdir()
        run("missing Data/Ps5 not a candidate", "scan", missing_data.parent, absent=True)

        target = make_game(base / "symlink-target/FC27")
        root = base / "symlink-root"
        root.mkdir()
        (root / "FC27").symlink_to(target, target_is_directory=True)
        run("symlink game is not followed", "scan", root, absent=True)
        run("manual symlink game rejected", "manual", root / "FC27", error="normal bir oyun klasoru")
        run("manual trailing slash cannot bypass symlink rejection", "manual", str(root / "FC27") + "/",
            error="normal bir oyun klasoru")
        run("symlink scan root not followed", "scan", root / "FC27", absent=True)
        alias = base / "redirected-parent"
        alias.symlink_to(target.parent, target_is_directory=True)
        run("symlink root ancestor not followed", "scan", alias / "FC27", absent=True)
        for relative in ("Data", "sce_sys", "ampr_emu.index"):
            game = make_game(base / f"symlink-{relative.replace('/', '-')}/FC27")
            original = game / relative
            destination = game / (relative + "-original")
            original.rename(destination)
            original.symlink_to(destination, target_is_directory=destination.is_dir())
            run(f"symlink {relative} rejected", "scan", game.parent, absent=True)

        root = base / "boundaries"
        game = make_game(root / "FC27")
        make_game(game / "Data/Ps5/fake-nested-game")
        run("game content is never recursively searched", "scan", root, expected=game)
        wrong = make_game(base / "other-game/Unrelated", title="PPSA00001")
        make_game(wrong / "Data/Ps5/FC27")
        run("other game content also not searched", "scan", wrong.parent, absent=True)
        root = base / "metadata"
        for name in (".Spotlight-V100", ".Trashes", "System Volume Information", "$RECYCLE.BIN"):
            make_game(root / name / "Fake game")
        real = make_game(root / "Real game")
        run("volume metadata skipped", "scan", root, expected=real)

        root = base / "limits"
        (root / "a").mkdir(parents=True)
        (root / "b").mkdir()
        run("directory budget fails safely", "limits", root, 1, 100, error="arama sinirina")
        run("entry budget fails safely", "limits", root, 100, 1, error="arama sinirina")
        # Inaccessible/nonexistent root does not fabricate a compressed-game
        # diagnosis or select a non-directory archive as a game.
        run("missing root ignored", "scan", base / "does-not-exist", absent=True)
        image = base / "game.ffpfsc"
        image.write_bytes(b"synthetic archive")
        run("image file not a game folder", "scan", image, absent=True)
        print(f"PASS {count} discovery cases; no game files or console were used.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
