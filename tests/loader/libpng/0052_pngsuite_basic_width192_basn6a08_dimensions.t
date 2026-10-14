#!/bin/sh
# Verify the requested dimensions after RGBA resizing.
# Reference generation: python3 tests/data/loader/pngsuite_expected/generate_sample_references.py

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png"
output_sixel="${TMPDIR:-/tmp}/libsixel-${0##*/}-$$-basn6a08.sixel"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 -Acomposite -B#000 -Nexplicit_first --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0  -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok 1 - RGBA resize failed"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_dimensions \
    "${output_sixel}" 192 192 || {
    echo "not ok 1 - RGBA resize changed the requested raster dimensions"
    exit 0
}
echo "ok 1 - RGBA output has the requested 192x192 dimensions"
exit 0
