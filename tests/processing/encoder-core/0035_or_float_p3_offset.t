#!/bin/sh
# Verify or float p3 offset with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0035_or_float_p3_offset || {
    echo "not ok 1 - or float p3 offset"
    exit 0
}
echo "ok 1 - or float p3 offset"
exit 0
