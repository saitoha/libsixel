#!/bin/sh
# Verify libpng indexed partial with an in-memory specimen.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng loader is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract indexed_partial || {
    echo "not ok 1 - libpng indexed partial"
    exit 0
}
echo "ok 1 - libpng indexed partial"
exit 0
