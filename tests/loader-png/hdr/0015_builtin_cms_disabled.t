#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify builtin cms disabled before output quantization.
set -eux
result=0
SIXEL_LOADER_HDR_SOURCE=auto SIXEL_LOADER_HDR_TONEMAP=auto \
    SIXEL_LOADER_HDR_REFERENCE_WHITE=203 SIXEL_LOADER_HDR_PEAK_LUMINANCE=1000 \
    SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=srgb-linear SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" png-hdr/0015_builtin_cms_disabled || result=$?
test "$result" -ne 77 || {
    echo '1..0 # SKIP loader unavailable'
    set -v
    exit 0
}
echo '1..1'
set -v
test "$result" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - builtin cms disabled'
    exit 0
}
printf '\n%s\n' 'ok 1 - builtin cms disabled'
exit 0
