#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify libpng hlg apng canvas before output quantization.
set -eux
result=0
SIXEL_LOADER_HDR_SOURCE=auto SIXEL_LOADER_HDR_TONEMAP=auto \
    SIXEL_LOADER_HDR_REFERENCE_WHITE=203 SIXEL_LOADER_HDR_PEAK_LUMINANCE=1000 \
    SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" png-hdr/0008_libpng_hlg_apng_canvas || result=$?
test "$result" -ne 77 || {
    echo '1..0 # SKIP loader unavailable'
    set -v
    exit 0
}
echo '1..1'
set -v
test "$result" -eq 0 || {
    echo 'not ok 1 - libpng hlg apng canvas'
    exit 0
}
echo 'ok 1 - libpng hlg apng canvas'
exit 0
