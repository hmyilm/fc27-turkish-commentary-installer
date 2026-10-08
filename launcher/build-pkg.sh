#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Usage: LIBPROSPERO_SOURCE=/path/to/pinned/source DOTNET=/path/to/dotnet \
#        ./launcher/build-pkg.sh <prepared-app-folder> <fresh-output-folder>
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo 'Usage: build-pkg.sh <prepared-app-folder> <fresh-output-folder>' >&2
  exit 2
fi
launcher_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
pkg_commit=748eabf1b7d17819528cabf367d8e27109d8fce3
pkg_source="${LIBPROSPERO_SOURCE:-$launcher_dir/.build/LibProsperoPKG}"
dotnet_command="${DOTNET:-dotnet}"
if [[ ! -f "$pkg_source/src/LibProsperoPkg/LibProsperoPkg.csproj" ]]; then
  mkdir -p -- "$(dirname -- "$pkg_source")"
  git clone --no-checkout https://github.com/SvenGDK/LibProsperoPKG.git "$pkg_source"
  git -C "$pkg_source" checkout --detach "$pkg_commit"
fi
if [[ -d "$pkg_source/.git" ]] && [[ "$(git -C "$pkg_source" rev-parse HEAD)" != "$pkg_commit" ]]; then
  echo 'LibProsperoPKG source does not match the pinned commit.' >&2
  exit 1
fi
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_GENERATE_ASPNET_CERTIFICATE=false
portable_source="$launcher_dir/.build/LibProsperoPKG-portable"
"${PYTHON:-python3}" "$launcher_dir/tools/prepare_pkg_library.py" "$pkg_source" "$portable_source"
"$dotnet_command" run --project "$launcher_dir/pkg-tool/PackageLauncher.csproj" \
  --configuration Release \
  --artifacts-path "$launcher_dir/.build/dotnet" \
  "-p:LibProsperoPkgProject=$portable_source/src/LibProsperoPkg/LibProsperoPkg.csproj" \
  -- "$1" "$2"
