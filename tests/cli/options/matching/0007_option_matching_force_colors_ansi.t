#!/bin/sh
# TAP test verifying forced color mode injects ANSI sequences in diagnostics.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/error.txt"
esc_char=$(printf '\033')

run_img2sixel --env SIXEL_STATUS_FORCE_COLORS=1 \
    -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "force colors diagnostic unexpectedly succeeded"
    exit 0
}

awk -v needle="${esc_char}[33m" 'index($0, needle) { found = 1; exit }
    END { exit found ? 0 : 1 }' "${err_file}" >/dev/null 2>&1 || {
    echo "not ok" 1 "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

awk -v needle="${esc_char}[1m" 'index($0, needle) { found = 1; exit }
    END { exit found ? 0 : 1 }' "${err_file}" >/dev/null 2>&1 || {
    echo "not ok" 1 "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

awk -v needle="${esc_char}[0m" 'index($0, needle) { found = 1; exit }
    END { exit found ? 0 : 1 }' "${err_file}" >/dev/null 2>&1 || {
    echo "not ok" 1 "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

echo "ok" 1 "force colors injects ANSI markers"
exit 0
