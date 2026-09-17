#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify exact coverage and independent opaque RGB under both K settings.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" png-keycolor/0001_frame_samples \
    "${TOP_SRCDIR}/images/pngsuite/transparency/tbrn2c08.png" || {
    echo "not ok 1 - keycolor frame samples"
    exit 0
}
echo "ok 1 - keycolor frame samples"
exit 0
