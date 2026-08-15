#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compose=(docker compose -f "$repo_root/windows-builder/compose.yml")
case "${1:-start}" in
  start)
    command -v docker >/dev/null || { echo 'Docker is required (with the Compose plugin).' >&2; exit 1; }
    test -e /dev/kvm || { echo '/dev/kvm is unavailable; enable CPU virtualization and load the KVM kernel module.' >&2; exit 1; }
    "${compose[@]}" up -d
    echo 'Open http://localhost:8006, then run Z:\windows-builder\build-release.bat in Windows.'
    ;;
  stop) "${compose[@]}" stop ;;
  down) "${compose[@]}" down ;;
  logs) "${compose[@]}" logs -f ;;
  status) "${compose[@]}" ps ;;
  *) echo "Usage: $0 {start|stop|down|logs|status}" >&2; exit 2 ;;
esac
