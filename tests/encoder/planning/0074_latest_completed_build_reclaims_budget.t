#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify a completed own build returns its budget before application.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0074_latest_completed_build_reclaims_budget 2>&1) || {
    echo "not ok 1 - latest completed build reclaims budget"
    echo "${trace}"
    exit 0
}
case "${trace}" in
    *'latest emit trace: frame=2 budget=8 emit=8 '*'latest emit trace: frame=2 budget=4 emit=4 '*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - latest planner lost the session or emission budget"
    echo "${trace}"
    exit 0
}
echo "ok 1 - latest completed build reclaims budget"
exit 0
