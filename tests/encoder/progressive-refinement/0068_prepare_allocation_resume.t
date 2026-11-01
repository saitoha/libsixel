#!/bin/sh
# Verify prepare allocation resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0068_prepare_allocation_resume" || {
    echo "not ok 1 - prepare allocation resume"
    exit 0
}
echo "ok 1 - prepare allocation resume"
exit 0
