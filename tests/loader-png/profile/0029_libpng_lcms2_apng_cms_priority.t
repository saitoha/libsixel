#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify ColorSync-compatible metadata selection with constant RGBA samples.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng comparison probes are unavailable"
    exit 0
}
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract \
    priority libpng lcms2 apng || result=$?

test "$result" -ne 77 || {
    echo "ok 1 # SKIP Little CMS is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - libpng lcms2 apng PNG metadata priority"
    exit 0
}
echo "ok 1 - libpng lcms2 apng PNG metadata priority"
exit 0
