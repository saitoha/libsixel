#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public statistics resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0095_public_statistics_resume" || {
    echo "not ok 1 - public statistics resume"
    exit 0
}
echo "ok 1 - public statistics resume"
exit 0
