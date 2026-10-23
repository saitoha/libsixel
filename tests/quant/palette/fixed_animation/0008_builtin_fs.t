#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: builtin fs.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0008_builtin_fs || {
    echo "not ok 1 - fixed animation builtin fs"
    exit 0
}

echo "ok 1 - fixed animation builtin fs"
exit 0
