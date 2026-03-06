#!/bin/sh
# TAP test: 16-bit sRGB PNG loader output should stay float32 through planner.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_64_rgb16_srgb_only_float32.sixel"

planner_log=$(
    set +xv
    run_img2sixel --env SIXEL_THREADS=4 -v \
                  -Llibpng:enable_cms=0! \
                  -o "${output_sixel}" "${input_png}" 2>&1
) || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

case "${planner_log}" in
    *"formats: source=rgb-f32 work=rgb-f32"*)
        ;;
    *)
        echo "not ok" 1 - "planner downgraded float32 input"
        exit 0
        ;;
esac

echo "ok" 1 - "planner keeps libpng rgb16 source in float32 work format"
exit 0
