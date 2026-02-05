#!/bin/sh
# Scale with Lanczos2 filter while disabling diffusion.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos2-no-diffusion.sixel"

if run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone \
        "${snake_jpg}" >"${target_sixel}"; then
    pass 1 "Lanczos2 scaling without diffusion succeeds"
else
    fail 1 "Lanczos2 scaling without diffusion fails"
fi

exit "${status}"
