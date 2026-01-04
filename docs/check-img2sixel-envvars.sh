#!/bin/sh
# Minimal harness that runs tests/docs/list_envvars.sh in check mode to
# ensure img2sixel -H stays in sync with the environment variables
# referenced by the sources.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

list_script="$repo_root/tests/docs/list_envvars.sh"
default_img2sixel="$repo_root/build/converters/img2sixel"
img2sixel=${IMG2SIXEL:-$default_img2sixel}

if [ ! -x "$list_script" ]; then
    echo "Helper script not found: $list_script" >&2
    exit 1
fi

if [ ! -x "$img2sixel" ]; then
    echo "img2sixel binary not found: $img2sixel" >&2
    echo "Build it with: meson compile -C build img2sixel" >&2
    exit 1
fi

echo "Checking img2sixel -H environment variable coverage..."
"$list_script" --check --img2sixel "$img2sixel" --source-root "$repo_root"
