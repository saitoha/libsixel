#!/bin/sh
# Scale with Lanczos2 filter while disabling diffusion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos2-no-diffusion.sixel"

run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone \
        "${snake_jpg}" >"${target_sixel}" || {
    fail 1 "Lanczos2 scaling without diffusion fails"
    exit 0
}

pass 1 "Lanczos2 scaling without diffusion succeeds"

exit 0
