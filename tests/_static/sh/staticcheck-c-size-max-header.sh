#!/bin/sh
# Emit TAP for SIZE_MAX header availability checks on tracked C sources.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if { test ! -d .git && test ! -f .git; } ||
        ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-size-max-header-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

# Exclude include fragments in the inventory itself, without another process.
# A failed Git command must not be hidden by a successful pipeline consumer.
git -c core.quotepath=false ls-files -- 'src/*.c' 'tests/**/*.c' \
    ':(exclude)*.inc.c' > "$tmpfile" || {
    echo "not ok 1 - failed to inventory tracked C source files"
    exit 1
}

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C source files"
    exit 0
fi

# Preserve deleted-file handling while scanning all existing inputs with one
# awk process. Explicit getline keeps empty files and file boundaries visible.
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue
    printf '%s\n' "$path"
done < "$tmpfile" | awk '
{
    path = $0
    uses_size_max = 0
    has_size_max_provider = 0
    while ((status = (getline line < path)) > 0) {
        # Literal gates avoid repeated regex work on ordinary C statements.
        # Still read the entire file so an input error cannot become a pass.
        if (!has_size_max_provider && index(line, "#") &&
                (line ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*<(limits|stdint)\.h>/ ||
                 line ~ /^[[:space:]]*#[[:space:]]*define[[:space:]]+SIZE_MAX([[:space:]]|$|\()/)) {
            has_size_max_provider = 1
        }
        if (!uses_size_max && index(line, "SIZE_MAX") &&
                line ~ /(^|[^[:alnum:]_])SIZE_MAX([^[:alnum:]_]|$)/) {
            uses_size_max = 1
        }
    }
    close(path)
    if (status < 0) {
        print "# " path ": unable to read tracked source"
        failed = 1
        next
    }
    if (!uses_size_max) next
    checked++
    if (!has_size_max_provider) {
        print "# " path ": uses SIZE_MAX but has no <limits.h>/<stdint.h> include or SIZE_MAX fallback define"
        failed = 1
    }
}
END {
    # Read failures take precedence over the no-usage skip verdict.
    if (failed) {
        print "not ok 1 - SIZE_MAX users provide required headers or fallback define"
    } else if (!checked) {
        print "ok 1 # SKIP no SIZE_MAX usage in tracked C source files"
    } else {
        print "ok 1 - SIZE_MAX users provide required headers or fallback define"
    }
    exit failed ? 1 : 0
}'
