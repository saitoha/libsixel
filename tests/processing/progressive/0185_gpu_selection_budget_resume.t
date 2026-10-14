#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Resume GPU selection and sampling with one-unit work budgets.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0185_gpu_selection_budget_resume"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu selection budget resume # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu selection budget resume"
    exit 0
}
echo "ok 1 - gpu selection budget resume"
exit 0
