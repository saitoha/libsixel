#!/bin/sh
# TAP test verifying path suggestions are emitted for missing files.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/path-suggestions-enabled.err"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 -- \
              -m "${TOP_SRCDIR}/tests/data/inputs/snake_64.pgn" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>"${err_file}" && {
    fail 1 "missing mapfile unexpectedly succeeded"
    exit 0
}

grep 'path "' "${err_file}" >/dev/null 2>&1 || {
    fail 1 "missing path suggestion diagnostics"
    exit 0
}

has_suggestions=1
grep 'Suggestions:' "${err_file}" >/dev/null 2>&1 || has_suggestions=0

test "${has_suggestions}" -eq 1 || {
    fail 1 "missing mapfile reports unsupported suggestion lookup"
    exit 0
}

pass 1 "missing mapfile prints ranked path suggestions"
exit 0
