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

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract \
    priority builtin builtin static || {
    echo "not ok 1 - builtin builtin static PNG metadata priority"
    exit 0
}
echo "ok 1 - builtin builtin static PNG metadata priority"
exit 0
