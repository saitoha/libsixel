#!/bin/sh
# Emit palette dump while performing interlaced encode.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/interlaced-palette-dump.sixel"

run_img2sixel -e -i -P "${snake_jpg}" >"${target_sixel}" || {
    fail 1 "interlaced encode palette dump fails"
    exit 0
}

pass 1 "interlaced encode emits palette dump"

exit 0
