#!/bin/sh
# Verify float monochrome lookup: encoder dark.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0008_encoder_dark || {
    echo "not ok 1 - mono lookup encoder dark"
    exit 0
}

echo "ok 1 - mono lookup encoder dark"
exit 0
