#!/bin/sh
# Emit TAP for header-comment/modeline checks on tracked C/C header files.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if test ! -d .git || ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-header-modeline-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

git -c core.quotepath=false ls-files -- 'src/*.c' 'src/*.h' > "$tmpfile" || true

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C/C header files"
    exit 0
fi

checked=0
failed=0
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue
    checked=$((checked + 1))

    spdx_line=$(sed -n '1,120p' "$path" | grep -m 1 'SPDX-License-Identifier:' \
        | sed 's/.*SPDX-License-Identifier:[[:space:]]*//')
    if test -z "$spdx_line"; then
        continue
    fi
    if test "$spdx_line" != "MIT"; then
        continue
    fi

    if ! sed -n '1,120p' "$path" | grep -q 'SPDX-License-Identifier: MIT'; then
        echo "# $path: missing SPDX license header"
        failed=1
    fi
    if ! sed -n '1,160p' "$path" | grep -q 'Permission is hereby granted'; then
        echo "# $path: missing MIT license grant"
        failed=1
    fi
    if ! sed -n '1,160p' "$path" | grep -q 'Copyright (c) '; then
        echo "# $path: missing copyright header"
        failed=1
    fi
    if ! tail -n 40 "$path" | grep -q 'emacs Local Variables'; then
        echo "# $path: missing emacs modeline near file end"
        failed=1
    fi
    if ! tail -n 40 "$path" | grep -q 'vim: set'; then
        echo "# $path: missing vim modeline near file end"
        failed=1
    fi
done < "$tmpfile"

if test "$checked" -eq 0; then
    echo "ok 1 # SKIP no MIT-licensed C/C header files"
    exit 0
fi

if test "$failed" -ne 0; then
    echo "not ok 1 - new C/C header files include header comment and modeline"
    exit 1
fi

echo "ok 1 - new C/C header files include header comment and modeline"
