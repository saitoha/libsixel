#!/bin/sh
# Float representatives remain attached to their keys across register redefinition passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0012_multipass_float || {
    echo "not ok 1 - high-color multipass float"
    exit 0
}
echo "ok 1 - high-color multipass float"
exit 0
