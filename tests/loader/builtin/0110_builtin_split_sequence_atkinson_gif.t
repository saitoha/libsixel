#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify the selected static GIF frame against independent source colors.
# Reference generation (ImageMagick):
#   magick 'tests/data/inputs/small.gif[0]' -alpha off -depth 8 \
#       PPM:tests/data/loader/builtin_expected/small-gif-first-samples.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -S -datkinson \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" || {
    echo "not ok" 1 - "sequence splitting with Atkinson fails"
    exit 0
}

# The first frame uses exact endpoint colors, with no palette loss.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/small-gif-first-samples.ppm" \
    "${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" 'builtin:Enone!' 0 || {
    echo "not ok 1 - static GIF frame colors differ from the independent reference"
    exit 0
}

echo "ok" 1 - "sequence splitting with Atkinson matches static reference"
exit 0
