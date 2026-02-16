#!/bin/sh
# Resize Sixel input while constraining palette size.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-resize.sixel"

run_img2sixel -w200 -p8 "${snake_six}" \
        >"${target_sixel}" || {
    fail 1 "Sixel resizing with palette limit fails"
    exit 0
}

pass 1 "Sixel resizing with palette limit succeeds"

exit 0
