#!/bin/sh
# Verify or float mono reject with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0039_or_float_mono_reject || {
    echo "not ok 1 - or float mono reject"
    exit 0
}
echo "ok 1 - or float mono reject"
exit 0
