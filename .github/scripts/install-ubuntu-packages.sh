#!/usr/bin/env bash
# Install Ubuntu packages with bounded mirror retries on hosted runners.

set -euo pipefail

test "$#" -gt 0 || {
  echo "Usage: $0 <package>..." >&2
  exit 2
}

packages=("$@")
apt_options=(
  -o Acquire::Retries=3
  -o Acquire::ForceIPv4=true
  -o Acquire::http::Timeout=30
  -o Acquire::https::Timeout=30
)

bash "${0%/*}/disable-hosted-runner-microsoft-apt-sources.sh"

apt_updated=0
for apt_attempt in 1 2 3; do
  if sudo timeout --kill-after=15s 4m \
      apt-get "${apt_options[@]}" update; then
    apt_updated=1
    break
  fi
  sleep $((apt_attempt * 10))
done

test "$apt_updated" -eq 1 || {
  echo "apt-get update failed after 3 bounded attempts" >&2
  exit 1
}

apt_installed=0
for apt_attempt in 1 2 3; do
  if sudo timeout --kill-after=15s 4m \
      apt-get "${apt_options[@]}" install -y --no-install-recommends \
        "${packages[@]}"; then
    apt_installed=1
    break
  fi
  sleep $((apt_attempt * 10))
done

test "$apt_installed" -eq 1 || {
  echo "apt-get install failed after 3 bounded attempts" >&2
  exit 1
}
