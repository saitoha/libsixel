#!/bin/sh
# Emit TAP for SIZE_MAX header availability checks on tracked C sources.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if test ! -d .git || ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-size-max-header-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

git -c core.quotepath=false ls-files -- 'src/*.c' > "$tmpfile" || true

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C source files"
    exit 0
fi

checked=0
failed=0
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue

    if awk '
    BEGIN {
        uses_size_max = 0
        has_size_max_provider = 0
    }
    /^[[:space:]]*#[[:space:]]*include[[:space:]]*<(limits|stdint)\.h>/ {
        has_size_max_provider = 1
    }
    /^[[:space:]]*#[[:space:]]*define[[:space:]]+SIZE_MAX([[:space:]]|$|\()/ {
        has_size_max_provider = 1
    }
    /(^|[^[:alnum:]_])SIZE_MAX([^[:alnum:]_]|$)/ {
        uses_size_max = 1
    }
    END {
        if (uses_size_max == 1) {
            if (has_size_max_provider == 1) {
                exit 0
            }
            exit 1
        }
        exit 2
    }
    ' "$path"; then
        checked=$((checked + 1))
    else
        rc=$?
        if test "$rc" -eq 2; then
            continue
        fi
        checked=$((checked + 1))
        failed=1
        echo "# $path: uses SIZE_MAX but has no <limits.h>/<stdint.h> include or SIZE_MAX fallback define"
    fi
done < "$tmpfile"

if test "$checked" -eq 0; then
    echo "ok 1 # SKIP no SIZE_MAX usage in tracked C source files"
    exit 0
fi

if test "$failed" -ne 0; then
    echo "not ok 1 - SIZE_MAX users provide required headers or fallback define"
    exit 1
fi

echo "ok 1 - SIZE_MAX users provide required headers or fallback define"
