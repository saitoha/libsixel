#!/bin/sh
# Verify named destination adobe reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0010_adobe_reuse || {
    echo "not ok 1 - named destination adobe reuse"
    exit 0
}

echo "ok 1 - named destination adobe reuse"
exit 0
