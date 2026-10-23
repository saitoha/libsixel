#!/bin/sh
# Verify named destination dci heckbert.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0022_dci_heckbert || {
    echo "not ok 1 - named destination dci heckbert"
    exit 0
}

echo "ok 1 - named destination dci heckbert"
exit 0
