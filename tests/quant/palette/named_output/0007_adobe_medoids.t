#!/bin/sh
# Verify named destination adobe medoids.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0007_adobe_medoids || {
    echo "not ok 1 - named destination adobe medoids"
    exit 0
}

echo "ok 1 - named destination adobe medoids"
exit 0
