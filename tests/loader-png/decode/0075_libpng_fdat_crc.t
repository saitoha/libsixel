#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify libpng fdat crc with an in-memory specimen.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng loader is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract fdat_crc || {
    echo "not ok 1 - libpng fdat crc"
    exit 0
}
echo "ok 1 - libpng fdat crc"
exit 0
