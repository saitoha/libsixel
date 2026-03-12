#!/bin/sh
# Emit TAP for codespell static check.

set -eu

echo "1..1"

src_root=$1
codespell_bin=$2

if test -n "$codespell_bin"; then
    cd "$src_root"
    find src tests \
        \( -path 'src/stb_image.h' -o -path 'src/stb_image_write.h' -o \
            -path 'tests/.python-test-venv' -o \
            -path 'tests/.ruby-test-venv' -o \
            -path 'tests/.perl-test-venv' -o \
            -path 'tests/.ruby-test-gem-home' \) -prune -o \
        -type f \( -name '*.[ch]' -o -name '*.md' -o \
            -name '*.1' -o -name '*.in' -o -name '*.am' -o \
            -name '*.build' -o -name '*.t' -o -name 'LICENSE' -o \
            -name '*.py' -o -name '*.rb' -o -name '*.pl' -o \
            -name '*.thumbnailer' -o -name '*.sh' \) \
        -exec "$codespell_bin" -L 'ser,sie' {} +
    echo "ok 1 - codespell"
else
    echo "ok 1 # SKIP codespell not found"
fi
