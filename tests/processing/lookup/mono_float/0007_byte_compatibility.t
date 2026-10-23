#!/bin/sh
# Verify float monochrome lookup: byte compatibility.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0007_byte_compatibility || {
    echo "not ok 1 - mono lookup byte compatibility"
    exit 0
}

echo "ok 1 - mono lookup byte compatibility"
exit 0
