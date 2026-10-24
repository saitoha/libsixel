#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

SIXEL_THREADING_GLOBAL_BUDGET=1 SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0010_adaptive_samples || {
    echo "not ok 1 - adaptive samples"
    exit 0
}
echo "ok 1 - adaptive samples"
exit 0
