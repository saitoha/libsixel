#!/bin/sh
# Encode existing Sixel data directly.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-direct.sixel"

if run_img2sixel -e "${snake_six}" >"${target_sixel}"; then
    pass 1 "direct Sixel encode emits data"
else
    fail 1 "direct Sixel encode failed"
fi

exit "${status}"
