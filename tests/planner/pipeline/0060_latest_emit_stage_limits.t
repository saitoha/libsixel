#!/bin/sh
# Verify the live emit allocation while frame 2's palette is in flight.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0060_latest_emit_stage_limits 2>&1) || {
    echo "not ok 1 - latest emit stage limits"
    echo "${trace}"
    exit 0
}
case "${trace}" in
    *'budget=8 '*'prepare_workers=8 '*'latest leases: palette=4 prepare=0 emit=8'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - latest planner lost the session or emission budget"
    echo "${trace}"
    exit 0
}
echo "ok 1 - latest emit stage limits"
exit 0
