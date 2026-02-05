#!/bin/sh
# Encode existing Sixel data directly.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

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
