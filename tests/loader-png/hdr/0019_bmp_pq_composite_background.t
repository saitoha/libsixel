#!/bin/sh
# Regression: bmp pq composite background; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
SIXEL_LOADER_HDR_SOURCE=auto \
    SIXEL_LOADER_BUILTIN_HDR_SOURCE=auto \
    SIXEL_LOADER_LIBPNG_HDR_SOURCE=auto \
    SIXEL_LOADER_HDR_TONEMAP=auto SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    SIXEL_LOADER_HDR_REFERENCE_WHITE=203 SIXEL_LOADER_HDR_PEAK_LUMINANCE=1000 \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=srgb-linear SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    png-hdr/0019_bmp_pq_composite_background || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - bmp pq composite background'
    exit 0
}
printf '\n%s\n' 'ok 1 - bmp pq composite background'
exit 0
