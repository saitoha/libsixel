#!/bin/sh
# High-color P3 non-gamma packed alpha rejects before output and allows opaque RGB reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0034_p3_alpha_space_recovery || {
    echo "not ok 1 - high-color p3 alpha space recovery"
    exit 0
}
echo "ok 1 - high-color p3 alpha space recovery"
exit 0
