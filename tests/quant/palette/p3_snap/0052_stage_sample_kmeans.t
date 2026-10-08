#!/bin/sh
# Verify numeric effects and actual execution of stage_sample_kmeans.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0002_pipeline stage sample 0 2>&1) || {
    echo "not ok 1 - stage_sample_kmeans: $trace"
    exit 0
}
case "$trace" in
    *'|sample='[1-9]*) ;;
    *) echo "not ok 1 - requested stage did not execute"; exit 0 ;;
esac
echo "ok 1 - stage_sample_kmeans"
exit 0
