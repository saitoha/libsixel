#!/bin/sh
# Verify step commit composition.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0078_step_commit_composition" || {
    echo "not ok 1 - step commit composition"
    exit 0
}
echo "ok 1 - step commit composition"
exit 0
