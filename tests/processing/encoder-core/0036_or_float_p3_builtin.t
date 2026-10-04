#!/bin/sh
# Verify or float p3 builtin with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0036_or_float_p3_builtin || {
    echo "not ok 1 - or float p3 builtin"
    exit 0
}
echo "ok 1 - or float p3 builtin"
exit 0
