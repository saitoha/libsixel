#!/bin/sh
# Regression: bmp pq keep no background; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
SIXEL_LOADER_HDR_SOURCE=auto \
    SIXEL_LOADER_BUILTIN_HDR_SOURCE=auto \
    SIXEL_LOADER_LIBPNG_HDR_SOURCE=auto \
    SIXEL_LOADER_HDR_TONEMAP=auto SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    SIXEL_LOADER_HDR_REFERENCE_WHITE=203 SIXEL_LOADER_HDR_PEAK_LUMINANCE=1000 \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    png-hdr/0022_bmp_pq_keep_no_background || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - bmp pq keep no background'
    exit 0
}
echo 'ok 1 - bmp pq keep no background'
exit 0
