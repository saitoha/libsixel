#!/bin/sh
# Verify palette budget resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0007_palette_budget_resume" || {
    echo "not ok 1 - palette budget resume"
    exit 0
}
echo "ok 1 - palette budget resume"
exit 0
