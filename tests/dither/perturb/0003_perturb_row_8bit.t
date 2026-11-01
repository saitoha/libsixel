#!/bin/sh
# Verify perturb row 8bit through the internal C contract.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0003_perturb_row_8bit" || {
    echo "not ok 1 - 0003_perturb_row_8bit"
    exit 0
}
echo "ok 1 - 0003_perturb_row_8bit"
exit 0
