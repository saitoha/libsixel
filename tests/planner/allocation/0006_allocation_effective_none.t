#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify effective allocation and occupied callback budgets.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    set -v
    exit 0
}
echo "1..1"
set -v

# The checked timeline needs a separate path for concurrent test processes.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
result=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0006_allocation_effective_none" \
    "${ARTIFACT_LOCAL_DIR}/allocation.jsonl" 2>&1) || {
    echo "not ok 1 - resolved allocation or phase ordering differs"
    echo "${result}"
    exit 0
}
case "${result}" in
    *'allocation: compiler disables parallel apply'*)
        echo "ok 1 # SKIP compiler disables parallel application"
        exit 0 ;;
esac

echo "ok 1 - resolved allocation and callback budget agree"
exit 0
