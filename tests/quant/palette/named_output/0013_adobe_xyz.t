#!/bin/sh
# Verify named destination adobe xyz.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0013_adobe_xyz || {
    echo "not ok 1 - named destination adobe xyz"
    exit 0
}

echo "ok 1 - named destination adobe xyz"
exit 0
