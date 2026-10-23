#!/bin/sh
# Verify named destination adobe center.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0006_adobe_center || {
    echo "not ok 1 - named destination adobe center"
    exit 0
}

echo "ok 1 - named destination adobe center"
exit 0
