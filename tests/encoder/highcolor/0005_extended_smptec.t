#!/bin/sh
# Negative working components contribute to the SMPTE-C primary matrix before final clipping.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0005_extended_smptec || {
    printf '\n%s\n' "not ok 1 - high-color extended smptec"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color extended smptec"
exit 0
