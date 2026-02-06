#!/bin/sh
# Resize Sixel input while constraining palette size.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-resize.sixel"

if run_img2sixel -w200 -p8 "${snake_six}" \
        >"${target_sixel}"; then
    pass 1 "Sixel resizing with palette limit succeeds"
else
    fail 1 "Sixel resizing with palette limit fails"
fi

exit "${status}"
