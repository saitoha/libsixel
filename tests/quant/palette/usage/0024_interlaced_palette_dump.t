#!/bin/sh
# Emit palette dump while performing interlaced encode.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/interlaced-palette-dump.sixel"

if run_img2sixel -e -i -P "${snake_jpg}" >"${target_sixel}"; then
    pass 1 "interlaced encode emits palette dump"
else
    fail 1 "interlaced encode palette dump fails"
fi

exit "${status}"
