#!/bin/sh
# pipeline minde oklab b4 exact bits.
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread support disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    gamut/0063_pipeline_bits 0 2 4 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - pipeline minde oklab b4 exact bits"
    exit 0
}
echo "ok 1 - pipeline minde oklab b4 exact bits"
exit 0
