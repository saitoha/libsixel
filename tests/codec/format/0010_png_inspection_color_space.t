#!/bin/sh
# Verify PNG inspection sets colour space in the report.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/png-inspection.txt"

if run_img2sixel -I -C10 -djajuni "${snake_png}" >"${target_txt}"; then
    pass 1 "PNG inspection sets colour space"
else
    fail 1 "PNG inspection colour space failed"
fi

exit "${status}"
