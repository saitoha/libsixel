#!/bin/sh
# TAP test verifying directory arguments are rejected for file-only options.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

err_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-rejects-directory.err")
out_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-rejects-directory.out")

run_img2sixel -m "${TOP_SRCDIR}/tests/data/inputs" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "directory mapfile unexpectedly succeeded"
    exit 0
}

grep -F 'path refers to a directory; expected a file input.' \
    "${err_file}" >/dev/null 2>&1 || {
    fail 1 "directory rejection diagnostic was not emitted"
    exit 0
}

pass 1 "directory arguments are rejected for file-only options"
exit 0
