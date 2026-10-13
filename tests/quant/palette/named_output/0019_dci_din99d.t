#!/bin/sh
# Verify named destination dci din99d.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0019_dci_din99d || {
    echo "not ok 1 - named destination dci din99d"
    exit 0
}

echo "ok 1 - named destination dci din99d"
exit 0
