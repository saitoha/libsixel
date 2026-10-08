#!/bin/sh
# Verify numeric effects and actual execution of stage_premerge_kmeans.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0002_pipeline stage premerge 0 2>&1) || {
    echo "not ok 1 - stage_premerge_kmeans: $trace"
    exit 0
}
case "$trace" in
    *'|premerge='[1-9]*) ;;
    *) echo "not ok 1 - requested stage did not execute"; exit 0 ;;
esac
echo "ok 1 - stage_premerge_kmeans"
exit 0
