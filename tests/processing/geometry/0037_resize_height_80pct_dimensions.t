#!/bin/sh
# Validate height scaling with the default bilinear resampler.
#
# Assert the requested height and preserved square aspect ratio directly.
# Resampler quality is covered by the separate 240-pixel filter cases.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"


data_root="${TOP_SRCDIR}/tests/data/inputs"
input_image="${data_root}/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/height_80pct.six"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -h 80% -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "height scaling with -h 80% failed"
    exit 0
}

# Percent dimensions truncate to whole pixels: 64 * 80 / 100
# when the option uses percent; bare integers and px are absolute sizes.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_dimensions \
    "${output_sixel}" 51 51 || {
    echo "not ok 1 - height option produced incorrect canvas dimensions"
    exit 0
}
echo "ok 1 - height option preserves the requested dimensions"
exit 0
