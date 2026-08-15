#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
request="$repo_root/windows-builder/build.request"
status="$repo_root/windows-builder/build.status"
log="$repo_root/windows-builder/build.log"
fingerprint="$repo_root/windows-builder/source.fingerprint"
timeout_seconds=${CTEXT_BUILD_TIMEOUT:-1800}

if [[ ! -d "$repo_root/windows-builder/windows" ]]; then
  echo 'The Windows VM has not been created. Run tools/windows-builder.sh start first.' >&2
  exit 1
fi

rm -f -- "$request" "$status" "$log"
# Shared-drive timestamp caching can make MSBuild miss changed modules. Give
# the guest an explicit source revision and let the batch file invalidate its
# local intermediates only when this revision changes.
tmp_fingerprint="$fingerprint.tmp"
find "$repo_root/ctext" "$repo_root/loader" -type f \( \
  -name '*.ixx' -o -name '*.cpp' -o -name '*.c' -o -name '*.h' -o -name '*.hpp' \
  -o -name '*.vcxproj' -o -name '*.filters' -o -name '*.json' -o -name '*.def' -o -name '*.rc' \
\) -print0 | sort -z | xargs -0 sha256sum | sha256sum | awk '{print $1}' > "$tmp_fingerprint"
mv -f -- "$tmp_fingerprint" "$fingerprint"
# The Dockur shared drive can briefly lag behind host writes.  Flush and give
# the bind mount a short settling window before the Windows watcher snapshots
# the source tree and starts incremental MSBuild.
sync
sleep 2
: > "$request"
echo 'Build requested. Waiting for the Windows VM...'

deadline=$((SECONDS + timeout_seconds))
state=""
while [[ "$state" != success && "$state" != failed ]]; do
  if (( SECONDS >= deadline )); then
    echo 'Timed out waiting for the Windows build watcher.' >&2
    echo 'Make sure the VM is running and the Docker user is logged in.' >&2
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
