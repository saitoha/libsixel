#!/bin/sh
# Verify PNG inspection sets colour space in the report.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/png-inspection.txt"

run_img2sixel -I -C10 -djajuni "${snake_png}" >"${target_txt}" || {
    fail 1 "PNG inspection colour space failed"
    exit 0
}

pass 1 "PNG inspection sets colour space"

exit 0
