#!/bin/sh
# Verify named destination dci snap recovery.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0025_dci_snap_recovery || {
    echo "not ok 1 - named destination dci snap recovery"
    exit 0
}

echo "ok 1 - named destination dci snap recovery"
exit 0
