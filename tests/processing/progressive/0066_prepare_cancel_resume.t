#!/bin/sh
# Verify prepare cancel resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0066_prepare_cancel_resume" || {
    echo "not ok 1 - prepare cancel resume"
    exit 0
}
echo "ok 1 - prepare cancel resume"
exit 0
