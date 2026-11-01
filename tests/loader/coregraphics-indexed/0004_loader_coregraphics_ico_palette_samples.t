#!/bin/sh
# Verify CoreGraphics preserves the original indexed ICO color samples.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Decode the ICO palette at native size. Source samples come from ImageMagick:
# magick 'tests/data/inputs/formats/snake-ico-pal8.ico[0]' -alpha off -depth 8 \
#   tests/data/inputs/formats/snake-ico-pal8-samples.ppm
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-pal8-samples.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-pal8.ico" 'coregraphics!' 0 || {
    echo "not ok 1 - ICO palette samples differ from the independent reference"
    exit 0
}
echo "ok 1 - ICO palette samples match exactly"
exit 0
