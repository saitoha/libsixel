#!/bin/sh
# Emit TAP for Makefile recipe tab validation.

set -eu

src_root=$1
tmpfile=`mktemp "${TMPDIR:-/tmp}/libsixel-makefiles-XXXXXX"`
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

cd "$src_root"
git ls-files -z -- 'Makefile.am' 'Makefile.in' '*/Makefile.am' '*/Makefile.in' > "$tmpfile"

echo "1..1"

if test ! -s "$tmpfile"; then
    echo "# makefile-recipes: no Makefile.am/Makefile.in tracked files found"
    echo "ok 1 - makefile recipes"
    exit 0
fi

if xargs -0 "$src_root/tools/check_makefile_recipes.sh" < "$tmpfile"; then
    echo "ok 1 - makefile recipes"
else
    echo "not ok 1 - makefile recipes"
    exit 1
fi
