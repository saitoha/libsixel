#!/bin/sh
# Scale with Lanczos2 filter while limiting palette size.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos2-palette-limit.sixel"

if run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "Lanczos2 scaling with palette limit succeeds"
else
    fail 1 "Lanczos2 scaling with palette limit fails"
fi

exit "${status}"
