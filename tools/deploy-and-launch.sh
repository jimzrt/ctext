#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
artifacts="$repo_root/windows-builder/artifacts"
game_dir="/home/james/.local/share/Steam/steamapps/common/Chrono Trigger"

files=(ctext.dll winmm.dll ChronoType.ttf)

if [[ ! -d "$game_dir" ]]; then
  echo "Chrono Trigger installation not found: $game_dir" >&2
  exit 1
fi

for file in "${files[@]}"; do
  if [[ ! -f "$artifacts/$file" ]]; then
    echo "Missing build artifact: $artifacts/$file" >&2
    echo "Build first with Z:\\windows-builder\\build-release.bat." >&2
    exit 1
  fi
done

if [[ ! -d "$artifacts/assets/cocos2d-ui" ]]; then
  echo "Missing packaged Cocos2d UI assets: $artifacts/assets/cocos2d-ui" >&2
  echo "Build first with Z:\\windows-builder\\build-release.bat." >&2
  exit 1
fi

cp -- "${files[@]/#/$artifacts/}" "$game_dir/"
mkdir -p "$game_dir/assets"
cp -a "$artifacts/assets/." "$game_dir/assets/"
# Keep user settings across builds and game restarts. Install the default only
# on first use; the in-game menu owns subsequent changes.
if [[ ! -f "$game_dir/ctext.json" ]]; then
  cp -- "$artifacts/ctext.json" "$game_dir/ctext.json"
fi
echo "Installed CText artifacts into: $game_dir"

if command -v steam >/dev/null 2>&1; then
  steam -applaunch 613830 >/dev/null 2>&1 &
elif command -v xdg-open >/dev/null 2>&1; then
  xdg-open steam://rungameid/613830 >/dev/null 2>&1 &
else
  echo "Steam launcher not found; start Chrono Trigger manually (App ID 613830)." >&2
  exit 1
fi
echo "Chrono Trigger launched."
