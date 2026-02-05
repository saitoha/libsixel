#!/bin/sh
# Inspect Sixel metadata successfully.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection.txt"

if run_img2sixel -I "${snake_six}" >"${target_txt}"; then
    pass 1 "Sixel metadata inspection succeeds"
else
    fail 1 "Sixel metadata inspection fails"
fi

exit "${status}"
