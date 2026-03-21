#!/bin/sh
# Emit TAP for codespell static check.

set -eu

src_root=$1
codespell_bin=$2

if test -z "$codespell_bin"; then
    echo "1..0 # SKIP codespell not found"
    exit 0
fi

if test ! -x "$codespell_bin" && ! command -v "$codespell_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP codespell executable not found: $codespell_bin"
    exit 0
fi

tmpfile=`mktemp "${TMPDIR:-/tmp}/libsixel-codespell-files-XXXXXX"`
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

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
        -name '*.thumbnailer' -o -name '*.sh' \) -print \
    | LC_ALL=C sort > "$tmpfile"

total=`wc -l < "$tmpfile" | awk '{print $1}'`

echo "1..1"

if test "$total" -eq 0; then
    echo "# codespell: no target files found"
    echo "ok 1 - codespell"
    exit 0
fi

failed=0
index=0
while IFS= read -r file_path; do
    test -n "$file_path" || continue
    index=$((index + 1))
    echo "# [codespell ${index}/${total}] $file_path"
    if ! "$codespell_bin" -L 'ser,sie' "$file_path"; then
        failed=1
    fi
done < "$tmpfile"

if test "$failed" -eq 0; then
    echo "ok 1 - codespell"
else
    echo "not ok 1 - codespell"
    exit 1
fi
