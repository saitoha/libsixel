#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Commit an admitted GPU repair before a waiting ordinary update.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0195_gpu_pending_after_admission"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu pending after admission # SKIP threads or Metal unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu pending after admission"
    exit 0
}
echo "ok 1 - gpu pending after admission"
exit 0
