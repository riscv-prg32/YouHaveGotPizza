#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
prg32_repo="${PRG32_REPO:-"$repo_dir/../PRG32"}"
stage_dir="$repo_dir/dist/store-bundle"
name="you-have-got-pizza"

mkdir -p "$stage_dir"
cp "$repo_dir/metadata/manifest.json" "$stage_dir/manifest.json"
cp "$repo_dir/assets/icon.png" "$stage_dir/icon.png"
cp "$repo_dir/assets/screenshot.png" "$stage_dir/screenshot.png"
cp "$repo_dir/dist/$name-esp32c6.prg32" "$stage_dir/$name-esp32c6.prg32"
cp "$repo_dir/dist/$name-qemu.prg32" "$stage_dir/$name-qemu.prg32"

(cd "$prg32_repo" && python3 -m prg32 store pack-bundle \
  --manifest "$stage_dir/manifest.json" \
  --out "$repo_dir/dist/$name-store-bundle.zip")

echo "$repo_dir/dist/$name-store-bundle.zip"
