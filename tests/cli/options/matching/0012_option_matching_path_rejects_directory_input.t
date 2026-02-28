#!/bin/sh
# TAP test verifying directory arguments are rejected for file-only options.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/path-rejects-directory.err"

run_img2sixel -m "${TOP_SRCDIR}/tests/data/inputs" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
              -o/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "directory mapfile unexpectedly succeeded"
    exit 0
}

grep 'path refers to a directory; expected a file input.' "${err_file}" >/dev/null 2>&1 || {
    echo "not ok" 1 "directory rejection diagnostic was not emitted"
    exit 0
}

echo "ok" 1 "directory arguments are rejected for file-only options"
exit 0
