#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: builtin workspace.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0006_builtin_workspace || {
    echo "not ok 1 - fixed animation builtin workspace"
    exit 0
}

echo "ok 1 - fixed animation builtin workspace"
exit 0
