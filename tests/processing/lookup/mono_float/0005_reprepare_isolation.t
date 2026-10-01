#!/bin/sh
# Verify float monochrome lookup: reprepare isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0005_reprepare_isolation || {
    echo "not ok 1 - mono lookup reprepare isolation"
    exit 0
}

echo "ok 1 - mono lookup reprepare isolation"
exit 0
