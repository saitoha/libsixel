#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation admitted deadline.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0144_presentation_admitted_deadline" || {
    echo "not ok 1 - presentation admitted deadline"
    exit 0
}
echo "ok 1 - presentation admitted deadline"
exit 0
