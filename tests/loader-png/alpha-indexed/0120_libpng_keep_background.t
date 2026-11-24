#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify libpng keep background with an in-memory specimen.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng loader is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract keep_background || {
    printf '\n%s\n' "not ok 1 - libpng keep background"
    exit 0
}
printf '\n%s\n' "ok 1 - libpng keep background"
exit 0
