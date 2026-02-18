#!/bin/sh
# TAP test verifying path suggestions report when no nearby entries exist.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

missing_path="${TOP_SRCDIR}/tests/quant/no-such-file.gpl"
err_file="${ARTIFACT_LOCAL_DIR}/path-suggestions-no-nearby.err"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -- \
              -m "${missing_path}" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>"${err_file}" && {
    fail 1 "missing mapfile unexpectedly succeeded"
    exit 0
}

has_nearby=1
grep 'No nearby matches were found in' "${err_file}" >/dev/null 2>&1 || has_nearby=0

test "${has_nearby}" -eq 1 || {
    fail 1 "missing no-nearby-matches diagnostic"
    exit 0
}

pass 1 "missing path reports unsupported suggestion lookup"
exit 0
