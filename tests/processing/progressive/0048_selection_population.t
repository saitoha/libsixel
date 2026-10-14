#!/bin/sh
# Verify selection population.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0048_selection_population" || {
    echo "not ok 1 - selection population"
    exit 0
}
echo "ok 1 - selection population"
exit 0
