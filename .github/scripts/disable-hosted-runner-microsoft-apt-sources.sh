#!/usr/bin/env bash
# Disable hosted-runner Microsoft apt sources that are unrelated to libsixel.

set -euo pipefail

source_files=()

while IFS= read -r source_file; do
  source_files+=("$source_file")
done < <(
  sudo grep -rl 'packages\.microsoft\.com' \
    /etc/apt/sources.list /etc/apt/sources.list.d 2>/dev/null || true
)

for source_file in "${source_files[@]}"; do
  if [ -f "$source_file" ]; then
    echo "Disabling hosted-runner Microsoft apt source: $source_file"
    sudo mv "$source_file" "$source_file.disabled"
  fi
done
