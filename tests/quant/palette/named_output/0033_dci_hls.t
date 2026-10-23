#!/bin/sh
# Verify named destination dci hls.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0033_dci_hls || {
    echo "not ok 1 - named destination dci hls"
    exit 0
}

echo "ok 1 - named destination dci hls"
exit 0
