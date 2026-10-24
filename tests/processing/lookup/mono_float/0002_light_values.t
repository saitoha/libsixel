#!/bin/sh
# Verify float monochrome lookup: light values.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0002_light_values || {
    echo "not ok 1 - mono lookup light values"
    exit 0
}

echo "ok 1 - mono lookup light values"
exit 0
