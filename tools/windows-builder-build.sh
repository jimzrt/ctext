#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
request="$repo_root/windows-builder/build.request"
status="$repo_root/windows-builder/build.status"
log="$repo_root/windows-builder/build.log"
timeout_seconds=${CTEXT_BUILD_TIMEOUT:-1800}

if [[ ! -d "$repo_root/windows-builder/windows" ]]; then
  echo 'The Windows VM has not been created. Run tools/windows-builder.sh start first.' >&2
  exit 1
fi

rm -f -- "$request" "$status" "$log"
# Publish the exact working tree the VM will build. The VM fetches this branch
# into a local checkout, so MSBuild can make normal per-file decisions.
if [[ -n "$(git -C "$repo_root" status --porcelain --untracked-files=all)" ]]; then
  git -C "$repo_root" add -A
  git -C "$repo_root" commit -m "Windows build snapshot $(date -u +%Y-%m-%dT%H:%M:%SZ)"
fi
git -C "$repo_root" push origin HEAD:build/ctext

sync
sleep 1
: > "$request"
echo 'Build requested. Waiting for the Windows VM...'

deadline=$((SECONDS + timeout_seconds))
state=""
while [[ "$state" != success && "$state" != failed ]]; do
  if (( SECONDS >= deadline )); then
    echo 'Timed out waiting for the Windows build watcher.' >&2
    echo 'The watcher heartbeat is missing or stale. Run Z:\\windows-builder\\install-watcher.bat once in the VM console.' >&2
    exit 1
  fi
  if [[ -f "$status" ]]; then
    state=$(tr -d '\r\n' < "$status")
  fi
  sleep 2
done

cat "$log" 2>/dev/null || true
if [[ "$state" == success ]]; then
  echo 'Windows build completed successfully.'
else
  echo 'Windows build failed.' >&2
  exit 1
fi
