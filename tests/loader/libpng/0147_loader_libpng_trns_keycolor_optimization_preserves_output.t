#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify keycolor modes preserve ColorType 2 samples and coverage.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libpng_contract keycolor_rgb8 || {
    echo "not ok 1 - keycolor modes changed ColorType 2 samples or coverage"
    exit 0
}

echo "ok 1 - keycolor modes preserve ColorType 2 samples and coverage"
exit 0
