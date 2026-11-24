#!/bin/sh
# Verify float monochrome lookup: reprepare isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0005_reprepare_isolation || {
    printf '\n%s\n' "not ok 1 - mono lookup reprepare isolation"
    exit 0
}

printf '\n%s\n' "ok 1 - mono lookup reprepare isolation"
exit 0
