#!/bin/sh
# Verify libwebp ICC pixelformat follows gamma cms target in loader test runner.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

SIXEL_LOADER_CMS_TARGET_COLORSPACE=gamma \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0017_loader_libwebp_pixelformat" >/dev/null || {
    echo "not ok" 1 - "loader/0017_loader_libwebp_pixelformat under cms target gamma"
    exit 0
}

echo "ok" 1 - "libwebp ICC loader pixelformat honors cms target gamma"
exit 0
