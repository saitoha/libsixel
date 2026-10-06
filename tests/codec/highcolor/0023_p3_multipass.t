#!/bin/sh
# P3 representatives survive register redefinition across completed passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0023_p3_multipass || {
    echo "not ok 1 - high-color p3 multipass"
    exit 0
}
echo "ok 1 - high-color p3 multipass"
exit 0
