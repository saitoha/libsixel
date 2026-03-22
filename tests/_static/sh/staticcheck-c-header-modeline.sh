#!/bin/sh
# Emit TAP for header-comment/modeline checks on newly added C/C header files.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if test ! -d .git || ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

base_ref=
if test -n "${GITHUB_BASE_REF:-}" \
        && git rev-parse --verify --quiet "origin/$GITHUB_BASE_REF" >/dev/null; then
    base_ref=`git merge-base HEAD "origin/$GITHUB_BASE_REF" 2>/dev/null || true`
fi
if test -z "$base_ref" \
        && git rev-parse --verify --quiet origin/develop >/dev/null; then
    base_ref=`git merge-base HEAD origin/develop 2>/dev/null || true`
fi
if test -z "$base_ref" \
        && git rev-parse --verify --quiet HEAD^ >/dev/null; then
    base_ref=`git rev-parse HEAD^`
fi

if test -z "$base_ref"; then
    echo "ok 1 # SKIP cannot determine baseline revision"
    exit 0
fi

tmpfile=`mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-header-modeline-XXXXXX"`
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

git -c core.quotepath=false diff --name-only --diff-filter=A \
    "$base_ref"...HEAD -- '*.c' '*.h' > "$tmpfile" || true

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no newly added C/C header files"
    exit 0
fi

checked=0
failed=0
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue
    checked=$((checked + 1))

    if ! sed -n '1,80p' "$path" | grep -q 'SPDX-License-Identifier: MIT'; then
        echo "# $path: missing SPDX license header"
        failed=1
    fi
    if ! sed -n '1,80p' "$path" | grep -Fq \
        'Copyright (c) 2026 libsixel developers. See `AUTHORS`.'; then
        echo "# $path: missing libsixel copyright header"
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
    echo "ok 1 # SKIP no newly added C/C header files"
    exit 0
fi

if test "$failed" -ne 0; then
    echo "not ok 1 - new C/C header files include header comment and modeline"
    exit 1
fi

echo "ok 1 - new C/C header files include header comment and modeline"
