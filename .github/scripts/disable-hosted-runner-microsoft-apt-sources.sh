#!/usr/bin/env bash
# Prepare hosted-runner apt sources for deterministic Ubuntu package installs.

set -euo pipefail

source_files=()
ubuntu_source_files=()

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

while IFS= read -r source_file; do
  ubuntu_source_files+=("$source_file")
done < <(
  sudo grep -rlE \
    'http://(azure\.)?archive\.ubuntu\.com|http://security\.ubuntu\.com|http://ports\.ubuntu\.com' \
    /etc/apt/apt-mirrors.txt /etc/apt/sources.list \
    /etc/apt/sources.list.d 2>/dev/null || true
)

for source_file in "${ubuntu_source_files[@]}"; do
  if [ -f "$source_file" ]; then
    echo "Switching hosted-runner Ubuntu apt source to HTTPS: $source_file"
    sudo sed -i \
      -e 's#http://azure\.archive\.ubuntu\.com/ubuntu#https://archive.ubuntu.com/ubuntu#g' \
      -e 's#http://archive\.ubuntu\.com/ubuntu#https://archive.ubuntu.com/ubuntu#g' \
      -e 's#http://security\.ubuntu\.com/ubuntu#https://security.ubuntu.com/ubuntu#g' \
      -e 's#http://ports\.ubuntu\.com/ubuntu-ports#https://ports.ubuntu.com/ubuntu-ports#g' \
      "$source_file"
  fi
done
