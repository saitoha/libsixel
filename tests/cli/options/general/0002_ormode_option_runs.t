#!/bin/sh
# Verify that img2sixel exits successfully when -O/--ormode is used.
set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake-ormode.sixel"

# LSQA cannot read ormode sixel output, so only check for a clean exit.
if run_img2sixel -O --outfile="${output_sixel}" <"${snake_jpg}" \
; then
    pass 1 "ormode option exits successfully"
else
    fail 1 "ormode option failed to run"
fi

exit "${status}"
