#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify libpng static alloc with an in-memory specimen.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng loader is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract static_alloc || {
    echo "not ok 1 - libpng static alloc"
    exit 0
}
echo "ok 1 - libpng static alloc"
exit 0
