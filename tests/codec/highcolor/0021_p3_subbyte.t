#!/bin/sh
# P3 output keeps a neutral subbyte representative through RGB101.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0021_p3_subbyte || {
    echo "not ok 1 - high-color p3 subbyte"
    exit 0
}
echo "ok 1 - high-color p3 subbyte"
exit 0
