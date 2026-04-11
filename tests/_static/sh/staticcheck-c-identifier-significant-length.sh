#!/bin/sh
# Emit TAP for C99 identifier significant-length checks on tracked C sources.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if test ! -d .git || ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-ident-len-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

git -c core.quotepath=false ls-files -- \
    'src/*.c' \
    'src/*.h' \
    'include/*.h' \
    'include/*.h.in' > "$tmpfile" || true

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C/C header files"
    exit 0
fi

failed=0
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue

    if ! awk '
    BEGIN {
        FS = "[^A-Za-z0-9_]+"
        limit = 63
        file_failed = 0
    }
    {
        for (i = 1; i <= NF; ++i) {
            token = $i
            if (token !~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
                continue
            }
            if (length(token) <= limit) {
                continue
            }
            if (token in seen) {
                continue
            }
            seen[token] = 1
            file_failed = 1
            printf("# %s:%d: identifier exceeds %d significant chars: %s (len=%d)\n",
                   FILENAME,
                   NR,
                   limit,
                   token,
                   length(token))
        }
    }
    END {
        exit file_failed ? 1 : 0
    }
    ' "$path"; then
        failed=1
    fi
done < "$tmpfile"

if test "$failed" -ne 0; then
    echo "not ok 1 - C identifiers stay within C99 significant-length limit"
    exit 1
fi

echo "ok 1 - C identifiers stay within C99 significant-length limit"
