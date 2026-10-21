#!/bin/sh
# Emit TAP for C99 identifier significant-length checks on tracked C sources.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if { test ! -d .git && test ! -f .git; } ||
        ! command -v git >/dev/null 2>&1; then
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
    'include/*.h.in' \
    'tests/**/*.c' \
    'tests/**/*.h' > "$tmpfile" || {
    echo "not ok 1 - failed to inventory tracked C/C header files"
    exit 1
}

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C/C header files"
    exit 0
fi

# Keep the deleted-file filter in shell builtins. Passing paths on stdin
# avoids ARG_MAX and starts only one awk, including in Git-file worktrees.
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue
    printf '%s\n' "$path"
done < "$tmpfile" | awk '
BEGIN { limit = 63 }
{
    path = $0
    number = 0
    # An identifier is reported once per file, never once per entire batch.
    # Explicit getline also handles empty files without carrying line state.
    delete seen
    while ((status = (getline line < path)) > 0) {
        number++
        # A short line cannot contain an over-limit token. Check token length
        # before its identifier grammar for the same reason.
        if (length(line) <= limit) continue
        count = split(line, fields, /[^A-Za-z0-9_]+/)
        for (i = 1; i <= count; ++i) {
            token = fields[i]
            if (length(token) <= limit ||
                    token !~ /^[A-Za-z_][A-Za-z0-9_]*$/ || token in seen) {
                continue
            }
            seen[token] = 1
            failed = 1
            printf("# %s:%d: identifier exceeds %d significant chars: %s (len=%d)\n",
                   path, number, limit, token, length(token))
        }
    }
    close(path)
    if (status < 0) {
        print "# " path ": unable to read tracked source"
        failed = 1
    }
}
END {
    if (failed) {
        print "not ok 1 - C identifiers stay within C99 significant-length limit"
    } else {
        print "ok 1 - C identifiers stay within C99 significant-length limit"
    }
    exit failed ? 1 : 0
}'
