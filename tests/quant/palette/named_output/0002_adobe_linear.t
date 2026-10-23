#!/bin/sh
# Verify named destination adobe linear.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0002_adobe_linear || {
    echo "not ok 1 - named destination adobe linear"
    exit 0
}

echo "ok 1 - named destination adobe linear"
exit 0
