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

# Missing package candidates can survive a successful update. Recheck the
# indexes before each install attempt so a retry can pick up a usable mirror.
for apt_attempt in 1 2 3; do
  if sudo timeout --kill-after=15s 4m \
      apt-get "${apt_options[@]}" -o APT::Update::Error-Mode=any update &&
      sudo timeout --kill-after=15s 4m \
        apt-get "${apt_options[@]}" install -y --no-install-recommends \
          "${packages[@]}"; then
    exit 0
  fi
  if test "$apt_attempt" -lt 3; then
    sleep $((apt_attempt * 10))
  fi
done

echo "apt-get could not update indexes and install packages after 3 attempts" >&2
exit 1
